#pragma once

#ifdef USE_RP2

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
#include "esphome/components/rp2040_ble/rp2040_ble.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstdint>

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::rp2_ble_tracker {

class RP2BLETracker : public Component,
                      public ble_device_base::BLEHub,
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
  void register_listener(ble_device_base::ESPBTDeviceListener *listener) override {
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
    this->listeners_.push_back(listener);
#endif
  }
  void set_raw_advertisement_callback(ble_device_base::RawAdvertisementCallback callback) override {
    this->raw_advertisement_callback_ = callback;
  }
  ble_device_base::HubCapabilities get_capabilities() const override {
    // BTstack delivers scan responses as separate advertisement reports rather
    // than merging them into the advertisement — consumers relying on
    // scan-response fields (device names) get them only where the receiver
    // merges per address (Home Assistant does). No GATT path yet.
    return {.active_scan = true, .merges_scan_response = false, .gatt = false};
  }
  // The controller stores the address in printable (MSB-first) order, which is
  // exactly what the contract wants.
  void get_adapter_mac(uint8_t out[6]) override { this->parent_->get_mac_msb_first(out); }
  bool scan_running() override { return this->scan_running_; }
  bool scan_active() override { return this->scan_active_; }
  bool request_scan_mode(bool active) override;

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

  ble_device_base::RawAdvertisementCallback raw_advertisement_callback_{};
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Parsed-advertisement consumers registered through ble_device_base.
  // Codegen-sized: no heap allocation, no std::vector template instantiations.
  StaticVector<ble_device_base::ESPBTDeviceListener *, ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT> listeners_;
  // Per-period "Found device" DEBUG log with MAC dedup — shared implementation
  // in ble_device_base, identical output on every tracker backend. Guarded like
  // its only writer so a no-listener build does not carry an unused vector.
  ble_device_base::DiscoveredDeviceLog discovered_log_{};
#endif
};

}  // namespace esphome::rp2_ble_tracker

#endif  // USE_RP2
