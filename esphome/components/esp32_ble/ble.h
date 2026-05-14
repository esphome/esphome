#pragma once

#include "esphome/core/defines.h"  // Must be included before conditional includes

#include "ble_uuid.h"
#include "ble_scan_result.h"
#ifdef USE_ESP32_BLE_ADVERTISING
#include "ble_advertising.h"
#endif

#include <functional>
#include <span>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "ble_event.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>

namespace esphome::esp32_ble {

// Maximum size of the BLE event queue
// Increased to absorb the ring buffer capacity from esp32_ble_tracker
#ifdef USE_PSRAM
static constexpr uint8_t MAX_BLE_QUEUE_SIZE = 100;  // 64 + 36 (ring buffer size with PSRAM)
#else
static constexpr uint8_t MAX_BLE_QUEUE_SIZE = 88;  // 64 + 24 (ring buffer size without PSRAM)
#endif

inline uint64_t ble_addr_to_uint64(const esp_bd_addr_t address) {
  uint64_t u = 0;
  u |= uint64_t(address[0] & 0xFF) << 40;
  u |= uint64_t(address[1] & 0xFF) << 32;
  u |= uint64_t(address[2] & 0xFF) << 24;
  u |= uint64_t(address[3] & 0xFF) << 16;
  u |= uint64_t(address[4] & 0xFF) << 8;
  u |= uint64_t(address[5] & 0xFF) << 0;
  return u;
}

// NOLINTNEXTLINE(modernize-use-using)
typedef struct {
  void *peer_device;
  bool connected;
  uint16_t mtu;
} conn_status_t;

enum IoCapability {
  IO_CAP_OUT = ESP_IO_CAP_OUT,
  IO_CAP_IO = ESP_IO_CAP_IO,
  IO_CAP_IN = ESP_IO_CAP_IN,
  IO_CAP_NONE = ESP_IO_CAP_NONE,
  IO_CAP_KBDISP = ESP_IO_CAP_KBDISP,
};

#ifdef ESPHOME_ESP32_BLE_EXTENDED_AUTH_PARAMS
enum AuthReqMode {
  AUTH_REQ_NO_BOND = ESP_LE_AUTH_NO_BOND,
  AUTH_REQ_BOND = ESP_LE_AUTH_BOND,
  AUTH_REQ_MITM = ESP_LE_AUTH_REQ_MITM,
  AUTH_REQ_BOND_MITM = ESP_LE_AUTH_REQ_BOND_MITM,
  AUTH_REQ_SC_ONLY = ESP_LE_AUTH_REQ_SC_ONLY,
  AUTH_REQ_SC_BOND = ESP_LE_AUTH_REQ_SC_BOND,
  AUTH_REQ_SC_MITM = ESP_LE_AUTH_REQ_SC_MITM,
  AUTH_REQ_SC_MITM_BOND = ESP_LE_AUTH_REQ_SC_MITM_BOND,
};
#endif

enum BLEComponentState : uint8_t {
  /** Nothing has been initialized yet. */
  BLE_COMPONENT_STATE_OFF = 0,
  /** BLE should be disabled on next loop. */
  BLE_COMPONENT_STATE_DISABLE,
  /** BLE is disabled. */
  BLE_COMPONENT_STATE_DISABLED,
  /** BLE should be enabled on next loop. */
  BLE_COMPONENT_STATE_ENABLE,
  /** BLE is active. */
  BLE_COMPONENT_STATE_ACTIVE,
};

class ESP32BLE : public Component {
 public:
  void set_io_capability(IoCapability io_capability) { this->io_cap_ = (esp_ble_io_cap_t) io_capability; }

#ifdef ESPHOME_ESP32_BLE_EXTENDED_AUTH_PARAMS
  void set_max_key_size(uint8_t key_size) { this->max_key_size_ = key_size; }
  void set_min_key_size(uint8_t key_size) { this->min_key_size_ = key_size; }
  void set_auth_req(AuthReqMode req) { this->auth_req_mode_ = (esp_ble_auth_req_t) req; }
#endif

  void set_advertising_cycle_time(uint32_t advertising_cycle_time) {
    this->advertising_cycle_time_ = advertising_cycle_time;
  }
  uint32_t get_advertising_cycle_time() const { return this->advertising_cycle_time_; }

  void enable();
  void disable();
  ESPHOME_ALWAYS_INLINE bool is_active() { return this->state_ == BLE_COMPONENT_STATE_ACTIVE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_name(const char *name) { this->name_ = name; }

#ifdef USE_ESP32_BLE_ADVERTISING
  void advertising_start();
  void advertising_set_service_data(const std::vector<uint8_t> &data);
  void advertising_set_manufacturer_data(const std::vector<uint8_t> &data);
  void advertising_set_appearance(uint16_t appearance) { this->appearance_ = appearance; }
  void advertising_set_service_data_and_name(std::span<const uint8_t> data, bool include_name);
  void advertising_add_service_uuid(ESPBTUUID uuid);
  void advertising_remove_service_uuid(ESPBTUUID uuid);
  void advertising_register_raw_advertisement_callback(std::function<void(bool)> &&callback);
#endif

#ifdef ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT
  template<typename F> void add_gap_event_callback(F &&callback) {
    this->gap_event_callbacks_.add(std::forward<F>(callback));
  }
#endif
#ifdef ESPHOME_ESP32_BLE_GAP_SCAN_EVENT_HANDLER_COUNT
  template<typename F> void add_gap_scan_event_callback(F &&callback) {
    this->gap_scan_event_callbacks_.add(std::forward<F>(callback));
  }
#endif
#if defined(USE_ESP32_BLE_CLIENT) && defined(ESPHOME_ESP32_BLE_GATTC_EVENT_HANDLER_COUNT)
  template<typename F> void add_gattc_event_callback(F &&callback) {
    this->gattc_event_callbacks_.add(std::forward<F>(callback));
  }
#endif
#if defined(USE_ESP32_BLE_SERVER) && defined(ESPHOME_ESP32_BLE_GATTS_EVENT_HANDLER_COUNT)
  template<typename F> void add_gatts_event_callback(F &&callback) {
    this->gatts_event_callbacks_.add(std::forward<F>(callback));
  }
#endif
#ifdef ESPHOME_ESP32_BLE_BLE_STATUS_EVENT_HANDLER_COUNT
  template<typename F> void add_ble_status_event_callback(F &&callback) {
    this->ble_status_event_callbacks_.add(std::forward<F>(callback));
  }
#endif
  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

 protected:
#ifdef USE_ESP32_BLE_SERVER
  static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif
#ifdef USE_ESP32_BLE_CLIENT
  static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
#endif
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  // Handle DISABLE and ENABLE transitions when not in the ACTIVE state.
  // Other non-ACTIVE states (e.g. OFF, DISABLED) are currently treated as no-ops.
  void __attribute__((noinline)) loop_handle_state_transition_not_active_();

  bool ble_setup_();
  bool ble_dismantle_();
  bool ble_pre_setup_();
#ifdef USE_ESP32_BLE_ADVERTISING
  void advertising_init_();
#endif

  // BLE uses the core wake_loop_threadsafe() mechanism to wake the main event loop
  // from BLE tasks. This enables low-latency (~12μs) event processing instead of
  // waiting for select() timeout (0-16ms). The wake socket is shared with other
  // components that need this functionality.

 private:
  template<typename... Args> friend void enqueue_ble_event(Args... args);

#ifdef ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT
  StaticCallbackManager<ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT,
                        void(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t *)>
      gap_event_callbacks_;
#endif
#ifdef ESPHOME_ESP32_BLE_GAP_SCAN_EVENT_HANDLER_COUNT
  StaticCallbackManager<ESPHOME_ESP32_BLE_GAP_SCAN_EVENT_HANDLER_COUNT, void(const BLEScanResult &)>
      gap_scan_event_callbacks_;
#endif
#if defined(USE_ESP32_BLE_CLIENT) && defined(ESPHOME_ESP32_BLE_GATTC_EVENT_HANDLER_COUNT)
  StaticCallbackManager<ESPHOME_ESP32_BLE_GATTC_EVENT_HANDLER_COUNT,
                        void(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t *)>
      gattc_event_callbacks_;
#endif
#if defined(USE_ESP32_BLE_SERVER) && defined(ESPHOME_ESP32_BLE_GATTS_EVENT_HANDLER_COUNT)
  StaticCallbackManager<ESPHOME_ESP32_BLE_GATTS_EVENT_HANDLER_COUNT,
                        void(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t *)>
      gatts_event_callbacks_;
#endif
#ifdef ESPHOME_ESP32_BLE_BLE_STATUS_EVENT_HANDLER_COUNT
  StaticCallbackManager<ESPHOME_ESP32_BLE_BLE_STATUS_EVENT_HANDLER_COUNT, void()> ble_status_event_callbacks_;
#endif

  // Large objects (size depends on template parameters, but typically aligned to 4 bytes)
  esphome::LockFreeQueue<BLEEvent, MAX_BLE_QUEUE_SIZE> ble_events_;
  // Pool sized to queue capacity (SIZE-1) because LockFreeQueue<T,N> is a ring
  // buffer that holds N-1 elements (one slot distinguishes full from empty).
  // This guarantees allocate() returns nullptr before push() can fail, which:
  //  1. Prevents leaking a pool slot (the Nth allocate succeeds but push fails)
  //  2. Avoids needing release() on the producer path after a failed push(),
  //     preserving the SPSC contract on the pool's internal free list
  esphome::EventPool<BLEEvent, MAX_BLE_QUEUE_SIZE - 1> ble_event_pool_;

  // 4-byte aligned members
#ifdef USE_ESP32_BLE_ADVERTISING
  BLEAdvertising *advertising_{};  // 4 bytes (pointer)
#endif
  const char *name_{nullptr};                 // 4 bytes (pointer to string literal in flash)
  esp_ble_io_cap_t io_cap_{ESP_IO_CAP_NONE};  // 4 bytes (enum)
  uint32_t advertising_cycle_time_{};         // 4 bytes

  // 2-byte aligned members
  uint16_t appearance_{0};  // 2 bytes

  // 1-byte aligned members (grouped together to minimize padding)
  BLEComponentState state_{BLE_COMPONENT_STATE_OFF};  // 1 byte (uint8_t enum)
  bool enable_on_boot_{};                             // 1 byte

#ifdef ESPHOME_ESP32_BLE_EXTENDED_AUTH_PARAMS
  optional<esp_ble_auth_req_t> auth_req_mode_;

  uint8_t max_key_size_{0};  // range is 7..16, 0 is unset
  uint8_t min_key_size_{0};  // range is 7..16, 0 is unset
#endif
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32BLE *global_ble;

template<typename... Ts> class BLEEnabledCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_ble != nullptr && global_ble->is_active(); }
};

template<typename... Ts> class BLEEnableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override {
    if (global_ble != nullptr)
      global_ble->enable();
  }
};

template<typename... Ts> class BLEDisableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override {
    if (global_ble != nullptr)
      global_ble->disable();
  }
};

}  // namespace esphome::esp32_ble

#endif
