// bk72xx_ble_tracker.h
//
// ESPHome BLE scanner for the BK72xx BLE-5.x chips (LibreTiny beken-72xx family).
// Implements the platform-neutral ble_device_base::BLEHub contract on top of the
// bk72xx_ble controller component: parsed ESPBTDevice objects go to registered
// listeners (bthome_mithermometer, ble_presence, …) and every raw frame to the
// hub's raw-advertisement callback.
//
// This component contains no Beken SDK calls and no cross-task state: the
// controller (stack bring-up, BLE address, scan primitives, and the BLE-task →
// main-task report queue) is owned by bk72xx_ble, which delivers every scan
// report on the ESPHome main task. The tracker owns scan policy — parameters,
// duration/period timers and the rate-limited start retry.
//
// YAML config (values shown are the defaults; interval/window are a 30 % duty
// cycle, the BK reference scan rate):
//
//   bk72xx_ble_tracker:
//     scan_parameters:
//       interval:   100ms
//       window:      30ms
//       duration:    5min
//       continuous:  true
//       active:      true

#pragma once

#ifdef USE_LIBRETINY

#include "esphome/components/bk72xx_ble/bk72xx_ble.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
#include "esphome/components/ble_device_base/scan_response_merger.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstdint>

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::bk72xx_ble_tracker {

// ---------------------------------------------------------------------------
// BK72xxBLETracker
// ---------------------------------------------------------------------------

class BK72xxBLETracker : public Component,
                         public bk72xx_ble::BLEScanListener,
                         public Parented<bk72xx_ble::BK72xxBLE>
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
  void set_scan_interval(uint32_t scan_interval) { this->scan_interval_ = scan_interval; }
  void set_scan_window(uint32_t scan_window) { this->scan_window_ = scan_window; }
  void set_scan_duration(uint32_t scan_duration) { this->scan_duration_ = scan_duration; }
  /// Set from YAML (scan_parameters.active); runtime mode requests change
  /// only the resolved mode.
  void set_scan_active(bool scan_active) {
    this->scan_active_ = scan_active;
    this->scan_active_configured_ = scan_active;
  }
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
    // Active scanning is driven through bk72xx_ble's reconciler because the BDK
    // API itself is passive-only. The controller delivers scan responses as
    // separate reports; this tracker merges the pair before delivery (shared
    // ScanResponseMerger, Bluedroid semantics). No GATT client.
    return {.active_scan = true, .merges_scan_response = true, .gatt = false, .scan_mode_switch = true};
  }
  bool request_scan_mode(bool active);
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

  // ---- bk72xx_ble::BLEScanListener ----
  // Delivered by the controller's loop() on the ESPHome main task — the
  // BLE-task → main-task handoff already happened in the controller's queue.
  void on_scan_report(const bk72xx_ble::BLEScanReport &report) override;

 protected:
  void start_scan_();
  void stop_scan_();
  void fire_scan_end_();
  void mark_scan_ended_(uint32_t now);
  /// Stamp-and-start for every controller scan attempt, so the retry rate
  /// limit covers all callers.
  bk72xx_ble::ScanOpResult controller_scan_start_();
  /// Rate-limited (re)start; true when the scan is running (the caller must
  /// not reuse a `now` older than the stamps this refreshed). Force and
  /// backoff rules are documented at the definition.
  bool try_start_with_backoff_(uint32_t now, bool force = false);
  void count_failed_start_();

  bool scan_running_{false};
  bool scan_requested_{false};      // latched start_scan() request not yet running; loop() retries with backoff
  bool start_attempt_open_{false};  // charge a later FAILED observation to the backoff exactly once
  // Defaults: the BK reference — 30 % duty cycle
  // (interval 100 ms / window 30 ms), in 0.625 ms BLE units.
  uint32_t scan_interval_{160};  // 160 × 0.625 ms = 100 ms
  uint32_t scan_window_{48};     // 48 × 0.625 ms = 30 ms (30/100 = 30 %)
  uint32_t scan_duration_{300000};
  bool scan_continuous_{true};
  bool scan_continuous_configured_{true};  // YAML value; stop_scan() must not lose it
  bool scan_active_{true};                 // resolved mode; see scan_parameters.active
  bool scan_active_configured_{true};      // YAML value; runtime requests must not lose it
#ifdef USE_OTA_STATE_LISTENER
  bool scan_continuous_before_ota_{false};  // continuous mode saved at OTA start, restored on OTA failure
  bool scan_requested_before_ota_{false};   // pending one-shot latch saved at OTA start, re-latched on OTA failure
#endif
  uint32_t scan_start_time_{0};

  uint32_t last_scan_start_attempt_{0};  // last controller start attempt, any caller; rate-limits retries
  uint8_t failed_start_count_{0};        // failed starts AND drops; backoff shift, cleared after a stable run (loop())
  uint32_t scan_period_start_{0};        // loop-clock start of the scan period; rate-limits on_scan_end()
  bool scan_started_once_{false};        // true after first successful scan start; gates the period timer

  // Shared adv + scan-response merge and frame dispatch (ble_device_base).
  // All calls run on the main task (the controller queue already crossed
  // tasks). Merger clock: stash_adv() reads the PARENT's cached loop time
  // (on_scan_report runs inside bk72xx_ble's queue drain), sweep() this
  // component's — same App.loop() pass, so the delta stays non-negative and
  // the 300 ms timeout holds.
  ble_device_base::ScanResponseMerger merger_;
  ble_device_base::AdvDispatcher dispatcher_;
};

}  // namespace esphome::bk72xx_ble_tracker

#endif  // USE_LIBRETINY
