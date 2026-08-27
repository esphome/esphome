// BLE scanner for LN882H: implements ble_device_base::BLEHub on top of the
// ln882h_ble controller (which owns all SDK calls and delivers scan reports on
// the main task). Scan policy lives here: parameters, period timers with
// per-period restart, and the adv+scan-response merge.

#pragma once

#ifdef USE_LIBRETINY

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
#include "esphome/components/ble_device_base/scan_response_merger.h"
#include "esphome/components/ln882h_ble/ln882h_ble.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstdint>

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::ln882h_ble_tracker {

// ---------------------------------------------------------------------------
// LN882HBLETracker
// ---------------------------------------------------------------------------

class LN882HBLETracker : public Component,
                         public Parented<ln882h_ble::LN882HBLE>,
                         public ln882h_ble::BLEScanListener
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
  // Pause scanning while an OTA update runs (single-core WiFi/BLE/flash contention);
  // mirrors esp32_ble_tracker.
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) override;
#endif

  // ---- YAML configuration setters ----
  void set_scan_active(bool scan_active) { this->scan_active_ = scan_active; }
  void set_scan_interval(uint16_t scan_interval) { this->scan_interval_ = scan_interval; }
  void set_scan_window(uint16_t scan_window) { this->scan_window_ = scan_window; }
  void set_scan_duration(uint32_t scan_duration) { this->scan_duration_ = scan_duration; }
  /// Set from YAML (scan_parameters.continuous); also the value
  /// configured_continuous() reports and a bare start_scan action restores.
  void set_configured_continuous(bool scan_continuous) {
    this->scan_continuous_ = scan_continuous;
    this->scan_continuous_configured_ = scan_continuous;
  }
  /// Runtime control (esp32_ble_tracker lambda parity): does not change the
  /// configured value, so configured_continuous() still reports what YAML
  /// asked for.
  void set_scan_continuous(bool scan_continuous) { this->scan_continuous_ = scan_continuous; }
  bool scan_continuous() const { return this->scan_continuous_; }
  bool configured_continuous() const { return this->scan_continuous_configured_; }
  /// Re-anchor the one-shot duration clock of a running scan to now — used
  /// when an action changes the scan mode without stopping the radio. The
  /// continuous-mode on_scan_end period is deliberately not touched.
  void restart_scan_duration();

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
    // The LN882H controller supports active scanning; adv + scan response arrive
    // as separate reports and are merged by this tracker (Bluedroid semantics).
    // The SDK's GATT client is not exposed.
    // scan_mode_switch: request_scan_mode() is implemented (restart-if-running).
    return {.active_scan = true, .merges_scan_response = true, .gatt = false, .scan_mode_switch = true};
  }
  // The controller stores the address LSB-first (BLE convention); the contract
  // wants printable (MSB-first) order.
  void get_adapter_mac(uint8_t out[MAC_ADDRESS_SIZE]) {
    uint8_t mac[MAC_ADDRESS_SIZE];
    this->parent_->get_mac_lsb_first(mac);
    for (int i = 0; i < 6; i++)
      out[i] = mac[5 - i];
  }
  bool scan_running() { return this->scan_running_; }
  bool scan_active() { return this->scan_active_; }
  bool request_scan_mode(bool active);

  // ---- ln882h_ble::BLEScanListener ----
  // Delivered by the controller's loop() on the ESPHome main task — the
  // rw-task → main-task handoff already happened in the controller's queue.
  // Demultiplexes advertisements vs scan responses and drives the merge.
  void on_scan_report(const ln882h_ble::BLEScanReport &report) override;

 protected:
  void start_scan_();
  void stop_scan_();
  // Close a scan period: flush held advertisements (unmerged) BEFORE
  // on_scan_end fires, then re-anchor the period clock to `now`.
  void end_scan_period_(uint32_t now);

  bool scan_running_{false};
  bool scan_active_{false};
  // Defaults are the LN882H SDK's recommended scan parameters
  // (ln_ble_scan.h: SCAN_INTERVAL_DEF 0xA0, SCAN_WINDOW_DEF 0x50 → 50 % duty).
  // uint16_t matches the controller's scan_start() parameters.
  uint16_t scan_interval_{160};  // 160 × 0.625 ms = 100 ms (SDK SCAN_INTERVAL_DEF)
  uint16_t scan_window_{80};     // 80 × 0.625 ms = 50 ms (SDK SCAN_WINDOW_DEF; 50/100 = 50 %)
  uint32_t scan_duration_{300000};
  bool scan_continuous_{true};
  bool pending_start_{false};              // start_scan() latched before the controller's setup()
  bool scan_continuous_configured_{true};  // YAML value; stop_scan() must not lose it
#ifdef USE_OTA_STATE_LISTENER
  bool scan_continuous_before_ota_{false};  // continuous mode saved at OTA start, restored on OTA failure
  bool scan_running_before_ota_{false};     // one-shot scan running at OTA start, restarted on OTA failure
#endif
  uint32_t scan_start_time_{0};

  // Shared adv + scan-response merge and frame dispatch (ble_device_base).
  // All calls run on the main task (the controller queue already crossed
  // tasks); the merger is clocked by millis() throughout this tracker.
  ble_device_base::ScanResponseMerger merger_;
  ble_device_base::AdvDispatcher dispatcher_;

  uint32_t scan_period_start_{0};  // millis() at start of current scan period; used to rate-limit on_scan_end()
};

}  // namespace esphome::ln882h_ble_tracker

#endif  // USE_LIBRETINY
