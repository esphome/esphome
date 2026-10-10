// rtl87xx_ble_tracker.h
//
// ESPHome BLE scanner for the BLE-capable Realtek chips (RTL8720C/D).
// Implements the platform-neutral ble_device_base::BLEHub contract on top of
// the rtl87xx_ble controller (Realtek GAP stack, btgap.a).
//
// The GAP callback runs in the BLE stack's task, so reports cross to the main
// loop through a lock-free SPSC ring before dispatch.
//
//   rtl87xx_ble_tracker:
//     scan_parameters:
//       interval:   100ms
//       window:      30ms
//       active:      true
//       continuous:  true

#pragma once

#ifdef USE_LIBRETINY

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/rtl87xx_ble/rtl87xx_ble.h"
#include "esphome/components/ble_device_base/ble_hub.h"
#include "esphome/components/ble_device_base/scan_response_merger.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

#include <atomic>
#include <cstdint>

namespace esphome::rtl87xx_ble_tracker {

// Raw scan report as it leaves the GAP task; sized for a full legacy AD payload.
struct RawReport {
  uint8_t mac[MAC_ADDRESS_SIZE];
  uint8_t addr_type;
  uint8_t adv_type;  // GAP advertising event type (Core spec values; 4 = SCAN_RSP)
  int8_t rssi;
  uint8_t len;
  uint8_t data[31];
};

// Single-producer (GAP task) / single-consumer (main loop) ring. Power-of-two
// size; a dropped report under overflow is preferable to blocking the stack.
static constexpr uint16_t REPORT_RING_SIZE = 64;

// Minimum interval between scan (re)start attempts, doubling per consecutive
// failure (1 s to 64 s) with one WARN at saturation. A stable run clears the
// streak; resetting on start would keep a flapping controller at the 1 s gate.
static constexpr uint32_t SCAN_RETRY_MS = 1000;
static constexpr uint8_t SCAN_START_RETRY_MAX_DOUBLINGS = 6;  // 1 s << 6 = 64 s
static constexpr uint32_t SCAN_STABLE_RESET_MS = 30000;

// The async stack bring-up normally completes within ~2 s of setup(); if it is
// not ready after this long it wedged (observed once on a post-OTA boot) and
// only a reboot recovers it — surface that instead of waiting silently.
static constexpr uint32_t STACK_READY_TIMEOUT_MS = 20000;
// Bounded inline drain of a controller-refused stop at OTA start (~100 ms
// ceiling), since the starved main loop cannot run the stop_pending_ retry.
static constexpr uint8_t OTA_STOP_RETRIES = 5;
static constexpr uint32_t OTA_STOP_RETRY_DELAY_MS = 20;

class RTL87xxBLETracker : public Component,
                          public Parented<rtl87xx_ble::RTL87xxBLE>
#ifdef USE_OTA_STATE_LISTENER
    ,
                          public ota::OTAGlobalStateListener
#endif
{
 public:
#ifdef USE_OTA_STATE_LISTENER
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) override;
#endif
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_scan_interval(uint32_t v) { this->scan_interval_ = v; }
  void set_scan_window(uint32_t v) { this->scan_window_ = v; }
  void set_scan_duration(uint32_t v) { this->scan_duration_ = v; }
  void set_scan_active(bool v) {
    this->scan_active_ = v;
    this->scan_active_configured_ = v;
  }
  void set_configured_continuous(bool v) {
    this->configured_continuous_ = v;
    this->scan_continuous_ = v;
  }
  void set_scan_continuous(bool v) { this->scan_continuous_ = v; }
  bool scan_continuous() const { return this->scan_continuous_; }
  bool configured_continuous() const { return this->configured_continuous_; }
  // Re-anchors the one-shot duration clock only (a same-mode call is a no-op),
  // leaving the period clock so repeated actions cannot starve on_scan_end.
  // scan_start_time_ also gates the drop detector's grace window and the
  // stable-run streak reset, so both restart with the run.
  void restart_scan_duration();

  // ---- BLEHub contract ----
  void register_listener(ble_device_base::ESPBTDeviceListener *listener) {
    this->dispatcher_.register_listener(listener);
  }
  void set_raw_advertisement_callback(ble_device_base::RawAdvertisementCallback callback) {
    this->dispatcher_.set_raw_advertisement_callback(callback);
  }
  static constexpr ble_device_base::HubCapabilities get_capabilities() {
    // Realtek GAP supports active scanning and a runtime mode switch
    // (le_scan_set_param). Scan responses arrive as separate reports, merged
    // here via ScanResponseMerger. No GATT client backend.
    return {.active_scan = true, .merges_scan_response = true, .gatt = false, .scan_mode_switch = true};
  }
  void get_adapter_mac(uint8_t out[MAC_ADDRESS_SIZE]);
  bool scan_running() { return this->scan_running_; }
  /// True while a controller-refused stop is being retried; the running flag
  /// stays set then, so actions must not treat the scan as simply running.
  bool stop_pending() const { return this->stop_pending_; }
  bool scan_active() { return this->scan_active_; }
  bool request_scan_mode(bool active);

  void start_scan();
  void stop_scan();

  // Called from the Realtek GAP task — push only, no ESPHome API here.
  void on_raw_report(const uint8_t mac[MAC_ADDRESS_SIZE], uint8_t addr_type, uint8_t adv_type, int8_t rssi,
                     const uint8_t *data, uint8_t len);

 protected:
  void apply_scan_params_();
  void count_failed_start_();
  /// Deliver held frames and fire on_scan_end; no state change.
  void fire_scan_end_();
  /// End the current scan period from every path where the scan really stops.
  void mark_scan_ended_(uint32_t now);
  void finish_stop_(uint32_t now);

  ble_device_base::AdvDispatcher dispatcher_;
  ble_device_base::ScanResponseMerger merger_;

  RawReport ring_[REPORT_RING_SIZE];
  std::atomic<uint16_t> ring_head_{0};  // producer (GAP task)
  std::atomic<uint16_t> ring_tail_{0};  // consumer (main loop)
  std::atomic<uint32_t> dropped_{0};    // ring overflow count (reported once per scan period in loop())
  uint32_t last_start_attempt_{0};
  uint32_t last_drop_report_{0};
  uint32_t scan_start_time_{0};    // one-shot duration clock (non-continuous stop)
  uint32_t scan_period_start_{0};  // continuous-mode on_scan_end period clock
  bool scan_started_once_{false};  // no spurious on_scan_end before the first real start

  uint32_t scan_duration_{300000};     // ms; scan_parameters.duration (default 5min)
  uint32_t scan_interval_{100};        // ms; the controller converts to 0.625 ms units
  uint32_t scan_window_{30};           // ms
  bool scan_active_{true};             // resolved mode; a runtime request_scan_mode() moves this
  bool scan_active_configured_{true};  // YAML scan_parameters.active; runtime requests must not lose it
  bool scan_continuous_{true};
  bool configured_continuous_{true};
  bool scan_running_{false};
  bool scan_requested_{false};     // latched start (stack not ready / start failed); loop() retries
  bool stop_pending_{false};       // controller refused a stop; loop() re-issues until it takes
  uint32_t last_stop_attempt_{0};  // rate-limits the stop_pending_ re-issue
  uint8_t failed_start_count_{0};  // failed starts AND drops; backoff shift, cleared after a stable run
  bool ble_started_{false};
  bool ready_timeout_logged_{false};
  bool network_wait_logged_{false};  // one-shot "waiting for the STA" notice
  uint32_t setup_time_{0};           // ready-timeout clock; pushed forward while the STA is down
  uint32_t last_drop_poll_{0};       // rate-limits the controller-state poll
  uint32_t boot_time_{0};            // setup() time, fixed: measures the STA wait itself
#ifdef USE_OTA_STATE_LISTENER
  bool scan_continuous_before_ota_{false};  // continuous mode saved at OTA start, restored on OTA failure
  bool scan_pending_before_ota_{false};     // one-shot scan in flight at OTA start, re-latched on OTA failure
#endif
};

}  // namespace esphome::rtl87xx_ble_tracker

#endif  // USE_LIBRETINY
