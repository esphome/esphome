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

// Local mirror of the co-processor's peer table. ESPHome's espnow component
// calls esp_now_is_peer_exist() on the main loop for every received frame
// (twice) and every send; forwarding each as a blocking RPC round-trip stalls
// the loop. The shim is the only path that mutates the co-processor peer table
// (add/del/deinit all go through here), so this mirror is authoritative and
// esp_now_is_peer_exist() can answer from it with no round-trip.
//
// esp_now_* are public C symbols: any component or user lambda may call them,
// and although ESPHome's espnow touches peers only from the main loop today
// (its RX/TX callbacks merely enqueue), the shim cannot rely on that. A short
// spinlock keeps the mirror consistent from any task/core, matching native
// esp_now_*'s own internal thread-safety. The critical sections are a bounded
// (<=20-entry) scan, so they stay tiny. ESP_NOW_MAX_TOTAL_PEER_NUM is 20.
constexpr size_t ESP_NOW_HOSTED_MAX_PEERS = 20;
uint8_t g_peer_cache[ESP_NOW_HOSTED_MAX_PEERS][6];
size_t g_peer_count = 0;
portMUX_TYPE g_peer_lock = portMUX_INITIALIZER_UNLOCKED;

// Caller must hold g_peer_lock.
int peer_cache_find_locked(const uint8_t *mac) {
  for (size_t i = 0; i < g_peer_count; i++) {
    if (memcmp(g_peer_cache[i], mac, 6) == 0)
      return static_cast<int>(i);
  }
  return -1;
}

bool peer_cache_contains(const uint8_t *mac) {
  portENTER_CRITICAL(&g_peer_lock);
  const bool found = peer_cache_find_locked(mac) >= 0;
  portEXIT_CRITICAL(&g_peer_lock);
  return found;
}

void peer_cache_add(const uint8_t *mac) {
  portENTER_CRITICAL(&g_peer_lock);
  if (peer_cache_find_locked(mac) < 0 && g_peer_count < ESP_NOW_HOSTED_MAX_PEERS)
    memcpy(g_peer_cache[g_peer_count++], mac, 6);
  portEXIT_CRITICAL(&g_peer_lock);
}

void peer_cache_remove(const uint8_t *mac) {
  portENTER_CRITICAL(&g_peer_lock);
  const int idx = peer_cache_find_locked(mac);
  if (idx >= 0) {
    g_peer_count--;
    if (static_cast<size_t>(idx) != g_peer_count)  // move the last entry into the gap
      memcpy(g_peer_cache[idx], g_peer_cache[g_peer_count], 6);
  }
  portEXIT_CRITICAL(&g_peer_lock);
}

void peer_cache_clear() {
  portENTER_CRITICAL(&g_peer_lock);
  g_peer_count = 0;
  portEXIT_CRITICAL(&g_peer_lock);
}

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
  if (rl > sizeof(g_resp_ret)) {
    // Larger than any real opcode return — a likely wire-format drift signal.
    ESP_LOGW(TAG, "RESP ret_len %u exceeds buffer, clamping (wire drift?)", rl);
    rl = sizeof(g_resp_ret);
  }
  if (len >= sizeof(esp_now_hosted_resp_t) + rl) {
    memcpy(g_resp_ret, r->ret, rl);
  } else {
    // Truncated frame: fail closed. Never hand the caller stale bytes left in
    // g_resp_ret by a previous response, and don't let request() report a
    // zeroed payload as success — override the status to an error.
    ESP_LOGW(TAG, "RESP truncated: claims %u ret bytes, frame too short", rl);
    rl = 0;
    g_resp_status = ESP_ERR_INVALID_RESPONSE;
  }
  g_resp_ret_len = rl;
  xSemaphoreGive(g_resp_sem);
}

void on_recv(uint32_t /*msg_id*/, const uint8_t *data, size_t len, void * /*ctx*/) {
  // Read the volatile pointer once: esp_now_unregister_recv_cb()/deinit() (via
  // the espnow component's disable()) can null it on the main loop between the
  // guard and the call, which would otherwise turn the call into a null-deref.
  const esp_now_recv_cb_t cb = g_recv_cb;
  if (cb == nullptr)
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
  cb(&info, e->data, static_cast<int>(e->data_len));
}

void on_send(uint32_t /*msg_id*/, const uint8_t *data, size_t len, void * /*ctx*/) {
  // Read the volatile pointer once (see on_recv): disable()/deinit() can null it
  // on the main loop concurrently with this RX-thread callback.
  const esp_now_send_cb_t cb = g_send_cb;
  if (cb == nullptr)
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
  cb(&si, static_cast<esp_now_send_status_t>(e->status));
#else
  cb(e->des_addr, static_cast<esp_now_send_status_t>(e->status));
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

// Send one request envelope. With wait=true (default) block until the matching
// response (or timeout); with wait=false return as soon as the frame is handed
// to the transport (fire-and-forget, used by esp_now_send).
//
// `tail` is an optional second chunk written straight after `payload`. Callers
// with a fixed header plus a bulk body (esp_now_send) pass the two separately
// so they never need a build buffer of their own: both chunks are laid into the
// request buffer here, under g_req_mutex, which keeps concurrent callers from
// racing and saves a full copy of the body on every transmit.
esp_err_t request(uint8_t opcode, const void *payload, uint16_t plen, void *ret, uint16_t ret_cap, uint16_t *ret_len,
                  bool wait = true, const void *tail = nullptr, uint16_t tail_len = 0) {
  esp_err_t err = ensure_setup();
  if (err != ESP_OK)
    return err;
  if (plen > ESP_NOW_HOSTED_MAX_PAYLOAD || tail_len > ESP_NOW_HOSTED_MAX_PAYLOAD - plen)
    return ESP_ERR_INVALID_SIZE;
  const uint16_t total_len = static_cast<uint16_t>(plen + tail_len);

  if (xSemaphoreTake(g_req_mutex, portMAX_DELAY) != pdTRUE)
    return ESP_FAIL;

  static uint8_t buf[sizeof(esp_now_hosted_req_t) + ESP_NOW_HOSTED_MAX_PAYLOAD];  // guarded by g_req_mutex
  auto *req = reinterpret_cast<esp_now_hosted_req_t *>(buf);
  req->opcode = opcode;
  req->seq = ++g_seq;
  req->payload_len = total_len;
  if (plen != 0)
    memcpy(req->payload, payload, plen);
  if (tail_len != 0)
    memcpy(req->payload + plen, tail, tail_len);
  g_expect_seq = req->seq;

  xSemaphoreTake(g_resp_sem, 0);  // drain any stale signal before sending
  err = esp_hosted_send_custom_data(ESP_NOW_HOSTED_MSG_REQ, buf, sizeof(esp_now_hosted_req_t) + total_len);
  if (err != ESP_OK) {
    xSemaphoreGive(g_req_mutex);
    return err;
  }
  if (!wait) {
    // Fire-and-forget (esp_now_send): the co-processor enqueues the frame and
    // reports the real TX result later via the async SEND event, exactly like
    // native esp_now_send. Returning here keeps the main loop off the ~100 ms+
    // RPC round-trip. The matching RESP is ignored (seq won't match the next
    // waited request, so on_resp drops it).
    xSemaphoreGive(g_req_mutex);
    return ESP_OK;
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
  peer_cache_clear();  // the co-processor drops all peers on deinit
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
  // Only arm the callback once the CustomRpc handlers are actually registered,
  // so a failed setup leaves g_recv_cb null rather than falsely "registered".
  esp_err_t err = ensure_setup();
  if (err != ESP_OK)
    return err;
  g_recv_cb = cb;
  return ESP_OK;
}
esp_err_t esp_now_unregister_recv_cb(void) {
  g_recv_cb = nullptr;
  return ESP_OK;
}
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t cb) {
  esp_err_t err = ensure_setup();
  if (err != ESP_OK)
    return err;
  g_send_cb = cb;
  return ESP_OK;
}
esp_err_t esp_now_unregister_send_cb(void) {
  g_send_cb = nullptr;
  return ESP_OK;
}

static esp_err_t add_or_mod_peer(uint8_t opcode, const esp_now_peer_info_t *peer, bool wait) {
  if (peer == nullptr)
    return ESP_ERR_ESPNOW_ARG;
  esp_now_hosted_peer_t p;
  memset(&p, 0, sizeof(p));
  memcpy(p.peer_addr, peer->peer_addr, 6);
  memcpy(p.lmk, peer->lmk, 16);
  p.channel = peer->channel;
  p.ifidx = static_cast<uint8_t>(peer->ifidx);
  p.encrypt = peer->encrypt ? 1 : 0;
  return request(opcode, &p, sizeof(p), nullptr, 0, nullptr, wait);
}
esp_err_t esp_now_add_peer(const esp_now_peer_info_t *peer) {
  // Fire-and-forget (wait=false): adding a peer is a blocking RPC round-trip,
  // and ESPHome's espnow calls it on the main loop when a device joins the mesh
  // — under co-processor load that stalls the UI (peer-churn stutter). Issue it
  // without waiting and mirror it locally. Safe against a following
  // esp_now_send to the same peer: both ride the same in-order CustomRpc
  // channel (mutex-serialized on the host) and the co-processor processes REQs
  // FIFO, so ADD_PEER is applied before the SEND. Trade-off: a co-processor-side
  // failure (e.g. peer table full) is no longer reported synchronously — the
  // same limitation as esp_now_send — but ESPHome only adds peers it validated.
  esp_err_t err = add_or_mod_peer(ESP_NOW_HOSTED_OP_ADD_PEER, peer, /*wait=*/false);
  if (err == ESP_OK)
    peer_cache_add(peer->peer_addr);  // keep the local mirror in sync
  return err;
}
esp_err_t esp_now_mod_peer(const esp_now_peer_info_t *peer) {
  // mod_peer changes a peer's parameters, not its existence, so the cache is
  // unaffected. Kept synchronous — it is not on any hot path (espnow never
  // calls it), so the extra round-trip does not matter and the status is useful.
  return add_or_mod_peer(ESP_NOW_HOSTED_OP_MOD_PEER, peer, /*wait=*/true);
}

esp_err_t esp_now_del_peer(const uint8_t *peer_addr) {
  if (peer_addr == nullptr)
    return ESP_ERR_ESPNOW_ARG;
  // Fire-and-forget for the same reason as add_peer (peer churn on the main
  // loop). Removal is order-independent, so this is strictly safe.
  esp_err_t err = request(ESP_NOW_HOSTED_OP_DEL_PEER, peer_addr, 6, nullptr, 0, nullptr, /*wait=*/false);
  if (err == ESP_OK)
    peer_cache_remove(peer_addr);  // keep the local mirror in sync
  return err;
}

bool esp_now_is_peer_exist(const uint8_t *peer_addr) {
  if (peer_addr == nullptr)
    return false;
  // Answered from the local mirror — no RPC round-trip. ESPHome's espnow calls
  // this on the main loop for every received frame and every send, so a
  // blocking round-trip here would stall rendering under mesh traffic.
  return peer_cache_contains(peer_addr);
}

esp_err_t esp_now_send(const uint8_t *peer_addr, const uint8_t *data, size_t len) {
  if (len > ESP_NOW_HOSTED_MAX_FRAME)
    return ESP_ERR_ESPNOW_ARG;
  if (data == nullptr && len != 0)  // native esp_now_send treats this as an arg error
    return ESP_ERR_ESPNOW_ARG;
  // Only the small fixed header is built here; the caller's frame goes over as
  // the request tail, so request() lays both into its own buffer under
  // g_req_mutex. esp_now_send is a public C symbol and may be called from any
  // task, and a shared build buffer here would let two callers corrupt each
  // other's frame. Passing the body through also drops a full-frame copy per
  // transmit, on the path this shim exists to keep quick.
  uint8_t hdr[sizeof(esp_now_hosted_send_req_t)];
  auto *s = reinterpret_cast<esp_now_hosted_send_req_t *>(hdr);
  s->has_addr = peer_addr != nullptr ? 1 : 0;
  if (peer_addr != nullptr)
    memcpy(s->peer_addr, peer_addr, 6);
  else
    memset(s->peer_addr, 0, 6);
  s->data_len = static_cast<uint16_t>(len);
  // Fire-and-forget (wait=false): native esp_now_send returns once the frame is
  // queued, with the real TX result delivered later through the send callback.
  // The co-processor mirrors that — it acks enqueue immediately and reports the
  // outcome via the async SEND event (on_send -> on_send_report). Waiting for
  // the RPC RESP here would block the main loop for the full round-trip on
  // every transmit.
  return request(ESP_NOW_HOSTED_OP_SEND, hdr, sizeof(hdr), nullptr, 0, nullptr, /*wait=*/false, data,
                 static_cast<uint16_t>(len));
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
