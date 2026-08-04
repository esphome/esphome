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

#pragma once

#ifdef USE_LIBRETINY

#include "esphome/components/bk72xx_ble/bk72xx_ble.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
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
                         public ble_device_base::BLEHub,
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
    // The Beken BDK exposes no active-scan path (passive scanning only), so the
    // controller never solicits scan responses and never merges them; consumers
    // relying on scan-response fields (device names) get them only where the
    // receiver merges per address (Home Assistant does). No GATT client either.
    return {.active_scan = false, .merges_scan_response = false, .gatt = false};
  }
  bool request_scan_mode(bool active) override {
    // Passive-only controller: a passive request is already honored, an active
    // one cannot be.
    return !active;
  }
  // The controller stores the address LSB-first (BLE convention); the contract
  // wants printable (MSB-first) order.
  void get_adapter_mac(uint8_t out[6]) override {
    uint8_t mac[6];
    this->parent_->get_mac_lsb_first(mac);
    for (int i = 0; i < 6; i++)
      out[i] = mac[5 - i];
  }
  bool scan_running() override { return this->scan_running_; }
  bool scan_active() override { return false; }  // BK72xx scan is passive-only

  // ---- bk72xx_ble::BLEScanListener ----
  // Delivered by the controller's loop() on the ESPHome main task — the
  // BLE-task → main-task handoff already happened in the controller's queue.
  void on_scan_report(const bk72xx_ble::BLEScanReport &report) override;

 protected:
  void start_scan_();
  void stop_scan_();
  /// Attempt a rate-limited (re)start; returns true when the scan is running,
  /// which means the caller must not compare its cached millis() against the
  /// timestamps start_scan_() just refreshed. force bypasses the rate gate for
  /// an explicit user start only while the failure streak is clean; a failing
  /// controller rate-limits forced attempts too. Failure accounting always runs.
  bool try_start_with_backoff_(uint32_t now, bool force = false);

  bool scan_running_{false};
  bool scan_requested_{false};  // latched start_scan() request not yet running; loop() retries with backoff
  // Defaults: the BK reference — 30 % duty cycle
  // (interval 100 ms / window 30 ms), in 0.625 ms BLE units.
  uint32_t scan_interval_{160};  // 160 × 0.625 ms = 100 ms
  uint32_t scan_window_{48};     // 48 × 0.625 ms = 30 ms (30/100 = 30 %)
  uint32_t scan_duration_{300000};
  bool scan_continuous_{true};
#ifdef USE_OTA_STATE_LISTENER
  bool scan_continuous_before_ota_{false};  // continuous mode saved at OTA start, restored on OTA failure
  bool scan_requested_before_ota_{false};   // pending one-shot latch saved at OTA start, re-latched on OTA failure
#endif
  uint32_t scan_start_time_{0};

  uint32_t last_scan_start_attempt_{0};  // millis() of last start_scan_() attempt; rate-limits retries
  uint8_t failed_start_count_{0};        // consecutive failed starts; drives the retry backoff (reset on success)
  uint32_t scan_period_start_{0};        // millis() at start of current scan period; used to rate-limit on_scan_end()
  bool scan_started_once_{false};        // true after first successful scan start; gates the period timer

  ble_device_base::RawAdvertisementCallback raw_advertisement_callback_{};
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Parsed-advertisement consumers registered through ble_device_base.
  // Codegen-sized: no heap allocation, no std::vector template instantiations.
  StaticVector<ble_device_base::ESPBTDeviceListener *, ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT> listeners_;
#endif

#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Per-period "Found device" DEBUG log with MAC dedup — shared implementation
  // in ble_device_base, identical output on every tracker backend. Guarded like
  // its only writer so a no-listener build does not carry an unused vector.
  ble_device_base::DiscoveredDeviceLog discovered_log_{};
#endif
};

}  // namespace esphome::bk72xx_ble_tracker

#endif  // USE_LIBRETINY
