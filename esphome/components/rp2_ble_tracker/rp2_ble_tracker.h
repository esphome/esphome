#pragma once

#ifdef USE_RP2

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
#include "esphome/components/ble_device_base/scan_response_merger.h"
#include "esphome/components/rp2040_ble/rp2040_ble.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstdint>

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::rp2_ble_tracker {

class RP2BLETracker : public Component,
                      public rp2040_ble::BLEScanListener,
                      public Parented<rp2040_ble::RP2040BLE>
#ifdef USE_OTA_STATE_LISTENER
    ,
                      public ota::OTAGlobalStateListener
#endif
{
 public:
  // ---- ESPHome Component ----
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_OTA_STATE_LISTENER
  // Pause scanning while an OTA update runs (the BLE scan competes with the OTA
  // download on the shared CYW43 radio); mirrors esp32_ble_tracker.
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) override;
#endif

  // ---- YAML configuration setters ----
  void set_scan_interval(uint32_t scan_interval) { this->scan_interval_ = scan_interval; }
  void set_scan_window(uint32_t scan_window) { this->scan_window_ = scan_window; }
  void set_scan_duration(uint32_t scan_duration) { this->scan_duration_ = scan_duration; }
  void set_scan_active(bool scan_active) { this->scan_active_ = scan_active; }
  void set_scan_continuous(bool scan_continuous) { this->scan_continuous_ = scan_continuous; }

  // ---- Public scan control ----
  // Mirrors esp32_ble_tracker: set_scan_continuous() + start_scan() / stop_scan().
  void start_scan();
  void stop_scan();

  // ---- ble_device_base::BLEHub contract ----
  void register_listener(ble_device_base::ESPBTDeviceListener *listener) {
    this->dispatcher_.register_listener(listener);
  }
  void set_raw_advertisement_callback(ble_device_base::RawAdvertisementCallback callback) {
    this->dispatcher_.set_raw_advertisement_callback(callback);
  }
  static constexpr ble_device_base::HubCapabilities get_capabilities() {
    // BTstack delivers scan responses as separate advertisement reports; this
    // tracker merges the pair before delivery (shared ScanResponseMerger,
    // Bluedroid semantics). GATT is available when the BTstack connection
    // backend is compiled in (bluetooth_proxy active).
#ifdef USE_BLE_GATT_CLIENT
    constexpr bool has_gatt = true;
#else
    constexpr bool has_gatt = false;
#endif
    return {.active_scan = true, .merges_scan_response = true, .gatt = has_gatt, .scan_mode_switch = true};
  }
  // The controller stores the address in printable (MSB-first) order, which is
  // exactly what the contract wants.
  void get_adapter_mac(uint8_t out[MAC_ADDRESS_SIZE]) { this->parent_->get_mac_msb_first(out); }
  bool scan_running() { return this->scan_running_; }
  bool scan_active() { return this->scan_active_; }
  bool request_scan_mode(bool active);

  // ---- rp2040_ble::BLEScanListener ----
  // Delivered by the controller's loop() on the ESPHome main loop — the
  // IRQ → main-loop handoff already happened in the controller's queue.
  void on_scan_report(const rp2040_ble::BLEScanReport &report) override;

 protected:
  void start_scan_();
  bool controller_scan_start_();
  void stop_scan_();
  void fire_scan_end_();

  // Defaults: 30 % duty cycle (interval 100 ms / window 30 ms), in 0.625 ms
  // BLE units — same defaults as bk72xx_ble_tracker.
  uint32_t scan_interval_{160};  // 160 × 0.625 ms = 100 ms
  uint32_t scan_window_{48};     // 48 × 0.625 ms = 30 ms (30/100 = 30 %)
  uint32_t scan_duration_{300000};
  uint32_t last_scan_start_attempt_{0};  // loop time of last start_scan_() attempt; rate-limits retries
  uint32_t scan_period_start_{0};        // loop time at start of current scan period; rate-limits on_scan_end()
  bool scan_running_{false};
  bool scan_active_{true};
  bool scan_continuous_{true};
#ifdef USE_OTA_STATE_LISTENER
  bool scan_continuous_before_ota_{false};  // continuous mode saved at OTA start, restored on OTA failure
  bool scan_pending_before_ota_{false};     // one-shot scan in flight at OTA start, resumed on OTA failure
#endif

  // Shared adv + scan-response merge and frame dispatch (ble_device_base).
  // All calls run on the main loop. Merger clock: stash_adv() reads the
  // PARENT's cached loop time (on_scan_report runs inside rp2040_ble's queue
  // drain), sweep() this component's — same App.loop() pass, so the delta
  // stays non-negative and the 300 ms timeout holds.
  ble_device_base::ScanResponseMerger merger_;
  ble_device_base::AdvDispatcher dispatcher_;
};

}  // namespace esphome::rp2_ble_tracker

#endif  // USE_RP2
