/*
 * esp_now_hosted — host-side shim implementing <esp_now.h> over esp-hosted
 * CustomRpc, so ESPHome's `espnow` component can run on a radio-less host
 * (e.g. the ESP32-P4) whose radio lives on an esp-hosted co-processor.
 *
 * A radio-less host has no native ESP-NOW. esp_wifi_remote INJECTS the full
 * esp_now.h header (types + declarations) but ships NO implementation, so every
 * esp_now_* symbol is an undefined reference at link time. This translation
 * unit provides those definitions; each forwards to the co-processor over
 * CustomRpc (see esphome/esp-hosted-firmware for the matching coprocessor
 * handlers). No esp-hosted or esp_wifi_remote source is patched, and there is no
 * duplicate-symbol clash because nothing else defines these symbols here.
 *
 * See esp_now_hosted_rpc.h for the wire protocol.
 */

#include "sdkconfig.h"

// Only build the shim on the radio-less host. On chips with a native ESP-NOW
// stack (S3, C6, …) the real symbols exist and this file must stay empty to
// avoid duplicate definitions.
#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <esp_now.h>         // injected declarations we are now DEFINING
#include <esp_wifi_types.h>  // wifi_pkt_rx_ctrl_t, wifi_tx_info_t

// esp_hosted_misc.h (host) ships WITHOUT an extern "C" guard, so including it
// from C++ would give its declarations C++ linkage and the real C symbols in
// libesp_hosted would go unresolved at link. Wrap it. (Verified vs
// esp_hosted 2.12.9.)
extern "C" {
#include "esp_hosted_misc.h"  // esp_hosted_{send_custom_data,register_custom_callback}
}

#include "esp_now_hosted_rpc.h"

namespace {

const char *const TAG = "esp_now_hosted";

// One outstanding request at a time. ESPHome drives esp_now_* from the main
// loop; the matching response and the async RECV/SEND events all arrive on the
// single esp-hosted RPC RX thread. Serializing requests keeps the shared
// response slot race-free; a sequence number stops a late/stale response from
// being mistaken for ours.
SemaphoreHandle_t g_req_mutex = nullptr;
SemaphoreHandle_t g_resp_sem = nullptr;  // given when the matching RESP lands
bool g_setup_done = false;               // set only after setup fully succeeds
uint8_t g_seq = 0;
volatile uint8_t g_expect_seq = 0;
volatile int32_t g_resp_status = 0;
uint8_t g_resp_ret[16];
volatile uint16_t g_resp_ret_len = 0;

// Written from the main loop (register/unregister/deinit), read from the
// esp-hosted RX thread (on_recv/on_send). volatile for the same reason the
// g_resp_* globals are: force the RX thread to observe an updated pointer
// (e.g. a nulling by esp_now_deinit) rather than a cached one.
volatile esp_now_recv_cb_t g_recv_cb = nullptr;
volatile esp_now_send_cb_t g_send_cb = nullptr;

// ── CustomRpc event handlers (run on the esp-hosted RPC RX thread) ──────────
// Keep them short and non-blocking. In particular they MUST NOT call back into
// any esp_now_* shim function: that would try to take g_req_mutex / wait on the
// RX thread that delivers the response, and deadlock.

void on_resp(uint32_t /*msg_id*/, const uint8_t *data, size_t len, void * /*ctx*/) {
  if (len < sizeof(esp_now_hosted_resp_t)) {
    ESP_LOGW(TAG, "RESP too short: %u bytes", static_cast<unsigned>(len));
    return;
  }
  const auto *r = reinterpret_cast<const esp_now_hosted_resp_t *>(data);
  if (r->seq != g_expect_seq) {  // late response from a timed-out request (expected)
    ESP_LOGV(TAG, "dropping stale RESP seq %u (want %u)", r->seq, g_expect_seq);
    return;
  }
  g_resp_status = r->status;
  uint16_t rl = r->ret_len;
  if (rl > sizeof(g_resp_ret))
    rl = sizeof(g_resp_ret);
  if (len >= sizeof(esp_now_hosted_resp_t) + rl) {
    memcpy(g_resp_ret, r->ret, rl);
  } else {
    // Truncated frame: fail closed. Never hand the caller stale bytes left in
    // g_resp_ret by a previous response — report zero return bytes instead.
    ESP_LOGW(TAG, "RESP truncated: claims %u ret bytes, frame too short", rl);
    rl = 0;
  }
  g_resp_ret_len = rl;
  xSemaphoreGive(g_resp_sem);
}

void on_recv(uint32_t /*msg_id*/, const uint8_t *data, size_t len, void * /*ctx*/) {
  if (g_recv_cb == nullptr)
    return;
  if (len < sizeof(esp_now_hosted_recv_evt_t)) {
    ESP_LOGW(TAG, "RECV too short: %u bytes", static_cast<unsigned>(len));
    return;
  }
  const auto *e = reinterpret_cast<const esp_now_hosted_recv_evt_t *>(data);
  if (len < sizeof(esp_now_hosted_recv_evt_t) + e->data_len) {
    ESP_LOGW(TAG, "RECV data_len %u exceeds frame", e->data_len);
    return;
  }

  // ESPHome dereferences info->rx_ctrl->{rssi,timestamp}; give it a real one.
  wifi_pkt_rx_ctrl_t rx_ctrl;
  memset(&rx_ctrl, 0, sizeof(rx_ctrl));
  rx_ctrl.rssi = e->rssi;
  rx_ctrl.channel = e->channel;
  rx_ctrl.timestamp = static_cast<uint32_t>(esp_timer_get_time());

  esp_now_recv_info_t info;
  info.src_addr = const_cast<uint8_t *>(e->src_addr);
  info.des_addr = const_cast<uint8_t *>(e->des_addr);
  info.rx_ctrl = &rx_ctrl;
  g_recv_cb(&info, e->data, static_cast<int>(e->data_len));
}

void on_send(uint32_t /*msg_id*/, const uint8_t *data, size_t len, void * /*ctx*/) {
  if (g_send_cb == nullptr)
    return;
  if (len < sizeof(esp_now_hosted_send_evt_t)) {
    ESP_LOGW(TAG, "SEND evt too short: %u bytes", static_cast<unsigned>(len));
    return;
  }
  const auto *e = reinterpret_cast<const esp_now_hosted_send_evt_t *>(data);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // IDF >= 5.5: esp_now_send_cb_t takes esp_now_send_info_t (== wifi_tx_info_t),
  // whose des_addr is a POINTER (not an inline array). Point it at the event's
  // MAC (valid for this callback) — do NOT memcpy into it (that writes NULL and
  // faults). ESPHome reads only info->des_addr.
  esp_now_send_info_t si;
  memset(&si, 0, sizeof(si));
  si.des_addr = const_cast<uint8_t *>(e->des_addr);
  g_send_cb(&si, static_cast<esp_now_send_status_t>(e->status));
#else
  g_send_cb(e->des_addr, static_cast<esp_now_send_status_t>(e->status));
#endif
}

esp_err_t ensure_setup() {
  // Gate on g_setup_done, not on g_req_mutex: a failure part-way through (a
  // semaphore that did not allocate, a callback that did not register) must not
  // leave a later call thinking setup completed. Semaphore creation is guarded
  // so a retry after a partial failure does not leak the earlier handles.
  if (g_setup_done)
    return ESP_OK;
  if (g_req_mutex == nullptr)
    g_req_mutex = xSemaphoreCreateMutex();
  if (g_resp_sem == nullptr)
    g_resp_sem = xSemaphoreCreateBinary();
  if (g_req_mutex == nullptr || g_resp_sem == nullptr)
    return ESP_ERR_NO_MEM;
  esp_err_t err;
  if ((err = esp_hosted_register_custom_callback(ESP_NOW_HOSTED_MSG_RESP, on_resp, nullptr)) != ESP_OK)
    return err;
  if ((err = esp_hosted_register_custom_callback(ESP_NOW_HOSTED_MSG_RECV, on_recv, nullptr)) != ESP_OK)
    return err;
  if ((err = esp_hosted_register_custom_callback(ESP_NOW_HOSTED_MSG_SEND, on_send, nullptr)) != ESP_OK)
    return err;
  g_setup_done = true;
  return ESP_OK;
}

// Send one request envelope and block until its response (or timeout).
esp_err_t request(uint8_t opcode, const void *payload, uint16_t plen, void *ret, uint16_t ret_cap, uint16_t *ret_len) {
  esp_err_t err = ensure_setup();
  if (err != ESP_OK)
    return err;
  if (plen > ESP_NOW_HOSTED_MAX_PAYLOAD)
    return ESP_ERR_INVALID_SIZE;

  if (xSemaphoreTake(g_req_mutex, portMAX_DELAY) != pdTRUE)
    return ESP_FAIL;

  static uint8_t buf[sizeof(esp_now_hosted_req_t) + ESP_NOW_HOSTED_MAX_PAYLOAD];  // guarded by g_req_mutex
  auto *req = reinterpret_cast<esp_now_hosted_req_t *>(buf);
  req->opcode = opcode;
  req->seq = ++g_seq;
  req->payload_len = plen;
  if (plen != 0)
    memcpy(req->payload, payload, plen);
  g_expect_seq = req->seq;

  xSemaphoreTake(g_resp_sem, 0);  // drain any stale signal before sending
  err = esp_hosted_send_custom_data(ESP_NOW_HOSTED_MSG_REQ, buf, sizeof(esp_now_hosted_req_t) + plen);
  if (err != ESP_OK) {
    xSemaphoreGive(g_req_mutex);
    return err;
  }
  if (xSemaphoreTake(g_resp_sem, pdMS_TO_TICKS(ESP_NOW_HOSTED_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGW(TAG, "opcode %u timed out", opcode);
    xSemaphoreGive(g_req_mutex);
    return ESP_ERR_TIMEOUT;
  }

  const int32_t status = g_resp_status;
  if (ret != nullptr && ret_cap != 0) {
    uint16_t n = g_resp_ret_len < ret_cap ? g_resp_ret_len : ret_cap;
    memcpy(ret, const_cast<const uint8_t *>(g_resp_ret), n);
    if (ret_len != nullptr)
      *ret_len = n;
  }
  xSemaphoreGive(g_req_mutex);
  return static_cast<esp_err_t>(status);
}

}  // namespace

// ── The <esp_now.h> surface, defined for the radio-less host ────────────────
extern "C" {

esp_err_t esp_now_init(void) { return request(ESP_NOW_HOSTED_OP_INIT, nullptr, 0, nullptr, 0, nullptr); }

esp_err_t esp_now_deinit(void) {
  g_recv_cb = nullptr;
  g_send_cb = nullptr;
  return request(ESP_NOW_HOSTED_OP_DEINIT, nullptr, 0, nullptr, 0, nullptr);
}

esp_err_t esp_now_get_version(uint32_t *version) {
  uint32_t v = 0;
  uint16_t rl = 0;
  esp_err_t err = request(ESP_NOW_HOSTED_OP_GET_VERSION, nullptr, 0, &v, sizeof(v), &rl);
  if (version != nullptr)
    *version = v;
  return err;
}

esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb) {
  g_recv_cb = cb;
  return ensure_setup();
}
esp_err_t esp_now_unregister_recv_cb(void) {
  g_recv_cb = nullptr;
  return ESP_OK;
}
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t cb) {
  g_send_cb = cb;
  return ensure_setup();
}
esp_err_t esp_now_unregister_send_cb(void) {
  g_send_cb = nullptr;
  return ESP_OK;
}

static esp_err_t add_or_mod_peer(uint8_t opcode, const esp_now_peer_info_t *peer) {
  if (peer == nullptr)
    return ESP_ERR_ESPNOW_ARG;
  esp_now_hosted_peer_t p;
  memset(&p, 0, sizeof(p));
  memcpy(p.peer_addr, peer->peer_addr, 6);
  memcpy(p.lmk, peer->lmk, 16);
  p.channel = peer->channel;
  p.ifidx = static_cast<uint8_t>(peer->ifidx);
  p.encrypt = peer->encrypt ? 1 : 0;
  return request(opcode, &p, sizeof(p), nullptr, 0, nullptr);
}
esp_err_t esp_now_add_peer(const esp_now_peer_info_t *peer) {
  return add_or_mod_peer(ESP_NOW_HOSTED_OP_ADD_PEER, peer);
}
esp_err_t esp_now_mod_peer(const esp_now_peer_info_t *peer) {
  return add_or_mod_peer(ESP_NOW_HOSTED_OP_MOD_PEER, peer);
}

esp_err_t esp_now_del_peer(const uint8_t *peer_addr) {
  if (peer_addr == nullptr)
    return ESP_ERR_ESPNOW_ARG;
  return request(ESP_NOW_HOSTED_OP_DEL_PEER, peer_addr, 6, nullptr, 0, nullptr);
}

bool esp_now_is_peer_exist(const uint8_t *peer_addr) {
  if (peer_addr == nullptr)
    return false;
  uint8_t exist = 0;
  uint16_t rl = 0;
  esp_err_t err = request(ESP_NOW_HOSTED_OP_IS_PEER_EXIST, peer_addr, 6, &exist, 1, &rl);
  if (err != ESP_OK) {
    // Distinguish a transport failure from a genuinely absent peer in the logs;
    // the native bool signature forces both to return false here.
    ESP_LOGW(TAG, "is_peer_exist RPC failed: %s", esp_err_to_name(err));
    return false;
  }
  return exist != 0;
}

esp_err_t esp_now_send(const uint8_t *peer_addr, const uint8_t *data, size_t len) {
  if (len > ESP_NOW_HOSTED_MAX_FRAME)
    return ESP_ERR_ESPNOW_ARG;
  if (data == nullptr && len != 0)  // native esp_now_send treats this as an arg error
    return ESP_ERR_ESPNOW_ARG;
  static uint8_t buf[sizeof(esp_now_hosted_send_req_t) + ESP_NOW_HOSTED_MAX_FRAME];  // guarded below
  // esp_now_send is only called from the main loop, so a plain static build
  // buffer is safe; request() then serializes the actual transmit.
  auto *s = reinterpret_cast<esp_now_hosted_send_req_t *>(buf);
  s->has_addr = peer_addr != nullptr ? 1 : 0;
  if (peer_addr != nullptr)
    memcpy(s->peer_addr, peer_addr, 6);
  else
    memset(s->peer_addr, 0, 6);
  s->data_len = static_cast<uint16_t>(len);
  if (len != 0)
    memcpy(s->data, data, len);
  return request(ESP_NOW_HOSTED_OP_SEND, buf, static_cast<uint16_t>(sizeof(esp_now_hosted_send_req_t) + len), nullptr,
                 0, nullptr);
}

esp_err_t esp_now_set_pmk(const uint8_t *pmk) {
  if (pmk == nullptr)
    return ESP_ERR_ESPNOW_ARG;
  return request(ESP_NOW_HOSTED_OP_SET_PMK, pmk, 16, nullptr, 0, nullptr);
}

// Remainder of the <esp_now.h> surface. Not used by ESPHome's espnow component
// today; provided so the whole header links and future callers get a defined
// (if unimplemented) symbol rather than a link error. Wire them through
// CustomRpc if a use case appears.
esp_err_t esp_now_get_peer(const uint8_t * /*peer_addr*/, esp_now_peer_info_t * /*peer*/) {
  return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t esp_now_fetch_peer(bool /*from_head*/, esp_now_peer_info_t * /*peer*/) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t esp_now_get_peer_num(esp_now_peer_num_t * /*num*/) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t esp_now_set_wake_window(uint16_t /*window*/) {
  return ESP_ERR_NOT_SUPPORTED;  // power-save wake window is not forwarded; don't claim success
}
esp_err_t esp_now_set_peer_rate_config(const uint8_t * /*peer_addr*/, esp_now_rate_config_t * /*cfg*/) {
  return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t esp_wifi_config_espnow_rate(wifi_interface_t /*ifx*/, wifi_phy_rate_t /*rate*/) {
  return ESP_ERR_NOT_SUPPORTED;
}

}  // extern "C"

#endif  // CONFIG_IDF_TARGET_ESP32P4
