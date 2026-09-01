#include "rtl87xx_ble.h"

#ifdef USE_RTL87XX_BLE

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <atomic>
#include <cstring>

#if defined(CLANG_TIDY)
// The clang-tidy environment has no Realtek GAP SDK, so there is nothing
// accurate to analyze the SDK calls against - skip the SDK-dependent body.
#define RTL87XX_BLE_NO_SDK
#elif !__has_include(<gap_scan.h>)
#define RTL87XX_BLE_NO_SDK
#error "rtl87xx_ble requires the Realtek GAP SDK headers (LibreTiny realtek-ambd or realtek-ambz2 core)"
#endif

#ifndef RTL87XX_BLE_NO_SDK

#include <libretiny.h>

// Vendor SDK headers: on the include path via the family builder's CONFIG_BT
// block on AmebaD, unconditionally on AmebaZ2.
extern "C" {
#include <FreeRTOS.h>
#include <task.h>

#include <gap.h>
#include <gap_callback_le.h>
#include <gap_config.h>
#include <gap_le.h>
#include <gap_le_types.h>
#include <gap_msg.h>
#include <gap_scan.h>

#include <app_msg.h>
#include <bte.h>
#include <os_msg.h>
#include <os_sched.h>
#include <os_task.h>
#include <rtk_coex.h>
#include <trace_app.h>
#include <wifi_conf.h>

int bt_get_mac_address(uint8_t *mac);
#ifdef USE_LIBRETINY_VARIANT_RTL8720D
// AmebaD only: AmebaZ2's coexistence is driven entirely through bt_coex_init()
// and the mailbox; its SDK has no wifi-side switch.
void wifi_btcoex_set_bt_on(void);
#endif

// bt_uart_tx: HCI debug-bridge TX, referenced by hci_uart.c but only used by
// the AT-command bridge that is not built. Weak stub keeps the link resolved.
__attribute__((weak)) void bt_uart_tx(uint8_t rc) { (void) rc; }
}

namespace esphome::rtl87xx_ble {

static const char *const TAG = "rtl87xx_ble";

// Only this TU sees the vendor headers, so the demux values are pinned here:
// a renumbered SDK fails the build instead of misrouting scan responses.
static_assert(ADV_EVENT_TYPE_ADV_IND == GAP_ADV_EVT_TYPE_UNDIRECTED, "GAP adv event type renumbered");
static_assert(ADV_EVENT_TYPE_ADV_SCAN_IND == GAP_ADV_EVT_TYPE_SCANNABLE, "GAP adv event type renumbered");
static_assert(ADV_EVENT_TYPE_SCAN_RSP == GAP_ADV_EVT_TYPE_SCAN_RSP, "GAP adv event type renumbered");

// SDK reference queue budget: the event queue must absorb both GAP messages
// and IO messages, so the three lengths are coupled - enforced below.
static constexpr size_t GAP_MSG_QUEUE_LEN = 0x20;
static constexpr size_t IO_QUEUE_LEN = 0x20;
static constexpr size_t EVT_QUEUE_LEN = GAP_MSG_QUEUE_LEN + IO_QUEUE_LEN;
static_assert(EVT_QUEUE_LEN == GAP_MSG_QUEUE_LEN + IO_QUEUE_LEN, "event queue must hold GAP + IO messages");
// 10 s at the 100 ms poll below; on hardware the stack is ready in under 1 s.
static constexpr uint16_t STACK_READY_TIMEOUT_TICKS = 100;
// One-shot notice after 30 s of waiting for the STA (100 ms ticks).
static constexpr uint32_t STA_WAIT_LOG_TICKS = 300;
// BLE spec scan interval/window range, in 0.625 ms units (2.5 ms .. 10.24 s).
static constexpr uint32_t SCAN_UNITS_MIN = 0x0004;
static constexpr uint32_t SCAN_UNITS_MAX = 0x4000;

// Bring-up order follows OpenBeken's hal_bt_proxy_rtl8720d.c: stack after the
// WiFi STA, coexistence enabled (bt_coex_init, plus wifi_btcoex_set_bt_on on
// AmebaD), GAP events pumped by a dedicated task. The same sequence compiles
// for AmebaZ2 (same Realtek GAP SDK); hardware-verified on AmebaD only.
//
// The GAP callbacks are plain C with no user argument, so state lives in
// file statics (one stack instance per SoC).
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// Written on the init task, read from the main task: atomic with
// release/acquire pairing, same discipline as s_adv_cb.
static std::atomic<bool> s_ble_ready{false};
static std::atomic<bool> s_ble_failed{false};
static bool s_ble_starting = false;  // main task only
static bool s_scan_running = false;  // main task only
// Written from the main loop, read from the GAP task; atomic so a clear
// (nullptr) can never be torn.
static std::atomic<raw_adv_callback_t> s_adv_cb{nullptr};

static uint16_t s_scan_interval_units = 0x520;  // 820 ms, the GAP reference default
static uint16_t s_scan_window_units = 0x520;
static uint8_t s_scan_mode = GAP_SCAN_MODE_PASSIVE;

static void *s_evt_queue = nullptr;
static void *s_io_queue = nullptr;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static T_APP_RESULT gap_callback(uint8_t cb_type, void *p_cb_data) {
  auto *p_data = static_cast<T_LE_CB_DATA *>(p_cb_data);
  if (p_data == nullptr || cb_type != GAP_MSG_LE_SCAN_INFO || p_data->p_le_scan_info == nullptr)
    return APP_RESULT_SUCCESS;
  T_LE_SCAN_INFO *info = p_data->p_le_scan_info;
  raw_adv_callback_t cb = s_adv_cb.load(std::memory_order_acquire);
  if (cb != nullptr) {
    cb(info->bd_addr, (uint8_t) info->remote_addr_type, (uint8_t) info->adv_type, (int8_t) info->rssi, info->data,
       info->data_len);
  }
  return APP_RESULT_SUCCESS;
}

static void pump_task(void *param) {
  (void) param;
  uint8_t event;
  if (!os_msg_queue_create(&s_io_queue, IO_QUEUE_LEN, sizeof(T_IO_MSG)) ||
      !os_msg_queue_create(&s_evt_queue, EVT_QUEUE_LEN, sizeof(uint8_t))) {
    ESP_LOGE(TAG, "GAP queue creation failed");
    s_ble_failed.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
    return;
  }
  gap_start_bt_stack(s_evt_queue, s_io_queue, GAP_MSG_QUEUE_LEN);
  while (true) {
    if (os_msg_recv(s_evt_queue, &event, 0xFFFFFFFF)) {
      if (event == EVENT_IO_TO_APP) {
        T_IO_MSG io_msg;
        os_msg_recv(s_io_queue, &io_msg, 0);
      } else {
        gap_handle_msg(event);
      }
    }
  }
}

static void init_task(void *param) {
  (void) param;

  // coex bring-up needs a running STA interface. The wait is deliberately
  // unbounded (a slow join must not hard-fail BLE), but it announces itself
  // once so an AP-only or never-connecting device is diagnosable from the log.
  for (uint32_t waited = 0; !wifi_is_up(RTW_STA_INTERFACE); waited++) {
    os_delay(100);
    if (waited == STA_WAIT_LOG_TICKS) {
      ESP_LOGW(TAG, "Still waiting for the WiFi STA before BLE bring-up");
    }
  }

  // Zero-initialized: this read happens before bte_init(), and a failed
  // le_get_gap_param() must not leave stack garbage that could fake
  // GAP_INIT_STATE_STACK_READY and skip the stack init entirely.
  T_GAP_DEV_STATE state = {};
  bt_trace_init();
  le_get_gap_param(GAP_PARAM_DEV_STATE, &state);
  if (state.gap_init_state != GAP_INIT_STATE_STACK_READY) {
    if (!bte_init()) {
      ESP_LOGE(TAG, "bte_init failed");
      s_ble_failed.store(true, std::memory_order_release);
      vTaskDelete(nullptr);
      return;
    }
    gap_config_max_le_link_num(2);
    if (!le_gap_init(2)) {
      ESP_LOGE(TAG, "le_gap_init failed");
      s_ble_failed.store(true, std::memory_order_release);
      vTaskDelete(nullptr);
      return;
    }
  }

  uint8_t filter_policy = GAP_SCAN_FILTER_ANY;
  uint8_t filter_duplicate = GAP_SCAN_FILTER_DUPLICATE_DISABLE;
  bool params_ok =
      le_scan_set_param(GAP_PARAM_SCAN_INTERVAL, sizeof(s_scan_interval_units), &s_scan_interval_units) ==
          GAP_CAUSE_SUCCESS &&
      le_scan_set_param(GAP_PARAM_SCAN_WINDOW, sizeof(s_scan_window_units), &s_scan_window_units) ==
          GAP_CAUSE_SUCCESS &&
      le_scan_set_param(GAP_PARAM_SCAN_MODE, sizeof(s_scan_mode), &s_scan_mode) == GAP_CAUSE_SUCCESS &&
      le_scan_set_param(GAP_PARAM_SCAN_FILTER_POLICY, sizeof(filter_policy), &filter_policy) == GAP_CAUSE_SUCCESS &&
      le_scan_set_param(GAP_PARAM_SCAN_FILTER_DUPLICATES, sizeof(filter_duplicate), &filter_duplicate) ==
          GAP_CAUSE_SUCCESS;
  if (!params_ok) {
    // Non-fatal: the stack still comes up on its own defaults.
    ESP_LOGW(TAG, "le_scan_set_param rejected a default");
  }

  le_register_app_cb(gap_callback);

  if (xTaskCreate(pump_task, "rtl_ble_pump", 1024, nullptr, 1, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "pump task creation failed");
    s_ble_failed.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
    return;
  }

  bt_coex_init();
  // Bounded: a stack that cannot come up (or a failed pump task) must surface
  // as a component failure, not an invisible spin.
  for (uint16_t waited = 0;; waited++) {
    os_delay(100);
    le_get_gap_param(GAP_PARAM_DEV_STATE, &state);
    if (state.gap_init_state == GAP_INIT_STATE_STACK_READY)
      break;
    if (s_ble_failed.load(std::memory_order_acquire)) {
      vTaskDelete(nullptr);
      return;
    }
    if (waited >= STACK_READY_TIMEOUT_TICKS) {
      ESP_LOGE(TAG, "BLE stack did not become ready");
      s_ble_failed.store(true, std::memory_order_release);
      vTaskDelete(nullptr);
      return;
    }
  }
#ifdef USE_LIBRETINY_VARIANT_RTL8720D
  wifi_btcoex_set_bt_on();
#endif
  os_delay(50);

  s_ble_ready.store(true, std::memory_order_release);
  ESP_LOGI(TAG, "BLE stack ready");
  vTaskDelete(nullptr);
}

// Pushes the current statics to the stack; main task only, so a concurrent
// set_scan_params() cannot be overwritten with stale values.
static void reapply_scan_params() {
  if (le_scan_set_param(GAP_PARAM_SCAN_INTERVAL, sizeof(s_scan_interval_units), &s_scan_interval_units) !=
          GAP_CAUSE_SUCCESS ||
      le_scan_set_param(GAP_PARAM_SCAN_WINDOW, sizeof(s_scan_window_units), &s_scan_window_units) !=
          GAP_CAUSE_SUCCESS ||
      le_scan_set_param(GAP_PARAM_SCAN_MODE, sizeof(s_scan_mode), &s_scan_mode) != GAP_CAUSE_SUCCESS) {
    ESP_LOGW(TAG, "le_scan_set_param failed; default scan parameters kept");
  }
}

void RTL87xxBLE::setup() {
#ifndef USE_WIFI
  // Bring-up follows the WiFi STA (coexistence requirement); without the wifi
  // component the init task would wait for it forever.
  ESP_LOGE(TAG, "rtl87xx_ble requires the wifi component");
  this->mark_failed();
  return;
#endif
  if (s_ble_ready.load(std::memory_order_acquire) || s_ble_starting)
    return;
  s_ble_starting = true;
  if (xTaskCreate(init_task, "rtl_ble_init", 1024, nullptr, 1, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "init task creation failed");
    this->mark_failed();
  }
}

void RTL87xxBLE::dump_config() {
  // Before the async bring-up completes, bt_get_mac_address() may not be
  // populated yet and would misreport the eFuse path as the WiFi fallback -
  // print the MAC only once the stack is up. The API re-runs dump_config() on
  // every client connect, so a running device does show it.
  if (!this->stack_ready()) {
    ESP_LOGCONFIG(TAG, "RTL87xx BLE:\n"
                       "  Stack ready: NO (bring-up pending; MAC reported once up)");
    return;
  }
  uint8_t mac[MAC_ADDRESS_SIZE];
  this->get_mac_msb_first(mac);
  ESP_LOGCONFIG(TAG,
                "RTL87xx BLE:\n"
                "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n"
                "  Stack ready: YES",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void RTL87xxBLE::loop() {
  if (s_ble_failed.load(std::memory_order_acquire)) {
    // The init/pump task already logged the specific cause.
    this->mark_failed();
    this->disable_loop();
  } else if (s_ble_ready.load(std::memory_order_acquire)) {
    // A set_scan_params() during bring-up only updated the statics (the
    // defaults were pushed before the readiness wait); re-apply here on the
    // main task, where no init-task write can race it.
    reapply_scan_params();
    this->disable_loop();
  }
}

bool RTL87xxBLE::stack_ready() const { return s_ble_ready.load(std::memory_order_acquire); }

// Same predicate init_task() blocks on, so callers see the wait it is in.
bool RTL87xxBLE::waiting_for_network() const {
  return !s_ble_ready.load(std::memory_order_acquire) && !wifi_is_up(RTW_STA_INTERFACE);
}

void RTL87xxBLE::set_adv_callback(raw_adv_callback_t cb) {
  raw_adv_callback_t prev = s_adv_cb.exchange(cb, std::memory_order_acq_rel);
  if (cb != nullptr && prev != nullptr && prev != cb) {
    // Single slot by design (one tracker per SoC); a displaced consumer would
    // otherwise just silently stop receiving reports.
    ESP_LOGW(TAG, "Advertisement callback replaced; previous consumer unregistered");
  }
}

void RTL87xxBLE::set_scan_params(uint16_t interval_ms, uint16_t window_ms, bool active) {
  // GAP units are 0.625 ms; 32-bit intermediate (the *16 product overflows
  // uint16_t above 4095 ms), clamped to the spec range, window <= interval.
  auto to_units = [](uint16_t ms) {
    uint32_t units = (static_cast<uint32_t>(ms) * 16) / 10;
    return static_cast<uint16_t>(std::clamp(units, SCAN_UNITS_MIN, SCAN_UNITS_MAX));
  };
  s_scan_interval_units = to_units(interval_ms);
  s_scan_window_units = std::min(to_units(window_ms), s_scan_interval_units);
  s_scan_mode = active ? GAP_SCAN_MODE_ACTIVE : GAP_SCAN_MODE_PASSIVE;
  if (s_ble_ready.load(std::memory_order_acquire)) {
    bool ok = le_scan_set_param(GAP_PARAM_SCAN_INTERVAL, sizeof(s_scan_interval_units), &s_scan_interval_units) ==
                  GAP_CAUSE_SUCCESS &&
              le_scan_set_param(GAP_PARAM_SCAN_WINDOW, sizeof(s_scan_window_units), &s_scan_window_units) ==
                  GAP_CAUSE_SUCCESS &&
              le_scan_set_param(GAP_PARAM_SCAN_MODE, sizeof(s_scan_mode), &s_scan_mode) == GAP_CAUSE_SUCCESS;
    if (!ok) {
      // The previous timings stay in effect; say so instead of scanning at
      // parameters the caller did not ask for.
      ESP_LOGW(TAG, "le_scan_set_param failed; previous scan parameters kept");
    }
  }
}

bool RTL87xxBLE::scan_start() {
  if (!s_ble_ready.load(std::memory_order_acquire) || s_scan_running)
    return s_scan_running;
  if (le_scan_start() != GAP_CAUSE_SUCCESS) {
    ESP_LOGW(TAG, "le_scan_start failed");
    return false;
  }
  s_scan_running = true;
  return true;
}

void RTL87xxBLE::scan_stop() {
  if (!s_ble_ready.load(std::memory_order_acquire) || !s_scan_running)
    return;
  if (le_scan_stop() != GAP_CAUSE_SUCCESS) {
    // Leave the flag set: the tracker's reconciler must see that the radio is
    // still scanning rather than trust a stop that did not happen.
    ESP_LOGW(TAG, "le_scan_stop failed");
    return;
  }
  s_scan_running = false;
}

bool RTL87xxBLE::scan_running() const {
  // The stack's own state, not a local flag, so a controller-side drop is
  // visible to the tracker's reconciler.
  if (!s_ble_ready.load(std::memory_order_acquire))
    return false;
  T_GAP_DEV_STATE state;
  if (le_get_gap_param(GAP_PARAM_DEV_STATE, &state) != GAP_CAUSE_SUCCESS)
    return s_scan_running;
  bool running = state.gap_scan_state == GAP_SCAN_STATE_START || state.gap_scan_state == GAP_SCAN_STATE_SCANNING;
  s_scan_running = running;
  return running;
}

void RTL87xxBLE::get_mac_msb_first(uint8_t out[MAC_ADDRESS_SIZE]) const {
  uint8_t mac[MAC_ADDRESS_SIZE] = {0};
  bt_get_mac_address(mac);
  if (!mac_address_is_valid(mac)) {
    // No usable BT eFuse slot (all-zero, all-ones or multicast garbage); use
    // the running WiFi MAC. lt_get_device_mac() reads the factory eFuse,
    // unprogrammed on some modules and then all-zero.
    get_mac_address_raw(out);
    return;
  }
  // bt_get_mac_address already returns printable order (the WiFi MAC's OUI with
  // the last byte offset), so it is copied as-is.
  memcpy(out, mac, MAC_ADDRESS_SIZE);
}

}  // namespace esphome::rtl87xx_ble

#endif  // RTL87XX_BLE_NO_SDK
#endif  // USE_RTL87XX_BLE
