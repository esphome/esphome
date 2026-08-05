// BLE scanner for LN882H: implements ble_device_base::BLEHub on top of the
// ln882h_ble controller (which owns all SDK calls and delivers scan reports on
// the main task). Scan policy lives here: parameters, period timers with
// per-period restart, and the adv+scan-response merge.

#pragma once

#ifdef USE_LIBRETINY

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
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
                         public ble_device_base::BLEHub,
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
  void register_listener(ble_device_base::ESPBTDeviceListener *listener) override {
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
    this->listeners_.push_back(listener);
#endif
  }
  void set_raw_advertisement_callback(ble_device_base::RawAdvertisementCallback callback) override {
    this->raw_advertisement_callback_ = callback;
  }
  ble_device_base::HubCapabilities get_capabilities() const override {
    // The LN882H controller supports active scanning; adv + scan response arrive
    // as separate reports and are merged by this tracker (Bluedroid semantics).
    // The SDK's GATT client is not exposed.
    // scan_mode_switch: request_scan_mode() is implemented (restart-if-running).
    return {.active_scan = true, .merges_scan_response = true, .gatt = false, .scan_mode_switch = true};
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
  bool scan_active() override { return this->scan_active_; }
  bool request_scan_mode(bool active) override;

  // ---- ln882h_ble::BLEScanListener ----
  // Delivered by the controller's loop() on the ESPHome main task — the
  // rw-task → main-task handoff already happened in the controller's queue.
  // Demultiplexes advertisements vs scan responses and drives the merge.
  void on_scan_report(const ln882h_ble::BLEScanReport &report) override;

 protected:
  // Bluedroid-style adv + scan-response merging (ESP-IDF concatenates both into
  // one result before ESPHome sees it; the LN controller reports them separately):
  // a scannable advertisement is held here briefly, its scan response is appended
  // on arrival and the pair is delivered as ONE merged frame. Held entries whose
  // scan response never arrives are flushed by loop() after PENDING_ADV_TIMEOUT_MS.
  // All of this runs on the main task (the controller queue already crossed tasks),
  // so no locking is involved.
  void stash_adv_(const ln882h_ble::BLEScanReport &report);
  void deliver_scan_rsp_(const ln882h_ble::BLEScanReport &report);
  // Dispatch one (possibly merged) advertisement: the raw
  // callback, and — unless raw_only — parsing for listeners/triggers. raw_only
  // marks unmatched scan-response frames: forwarded on the raw callback only,
  // never to local sensors/triggers (HA merges per address).
  void process_adv_(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data, uint8_t data_len,
                    bool raw_only);
  void start_scan_();
  void stop_scan_();
  // Close a scan period: flush held advertisements (unmerged) BEFORE
  // on_scan_end fires, then re-anchor the period clock to `now`.
  void end_scan_period_(uint32_t now);
  void flush_pending_adv_();

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

  // Pending scannable advertisements awaiting their scan response (active scan).
  // 62 bytes = legacy adv (31) + scan response (31), the same merged maximum as
  // ESP-IDF delivers on ESP32. Main-task only.
  struct PendingAdv {
    bool used{false};
    uint8_t mac[6];
    uint8_t addr_type;
    int8_t rssi;
    uint8_t data_len;  // <= sizeof(data)
    uint8_t data[62];
    uint32_t stored_ms;
  };
  // Sized for the unanswered case: a pair that IS answered normally matches
  // within one queue drain, so a slot is held for the full timeout only by
  // scannable devices that never reply. 8 concurrent such advertisers before
  // the merge degrades (frames still delivered, just unmerged) at ~80 B each.
  static constexpr size_t MAX_PENDING_ADV = 8;
  // On air a scan response follows its advertisement by T_IFS (150 µs) — the
  // timeout only covers HOST-side report queuing in rw_task under WiFi/BLE
  // coexistence, measured on-device at up to ~136 ms. 300 ms = >2x that margin,
  // while staying below any device's re-advertising period.
  static constexpr uint32_t PENDING_ADV_TIMEOUT_MS = 300;
  PendingAdv pending_adv_[MAX_PENDING_ADV];
  // Occupied pending_adv_ slots — lets loop()'s timeout sweep skip the table
  // in the common case (empty: passive scan, or every pair already matched).
  uint8_t pending_count_{0};

  uint32_t scan_period_start_{0};  // millis() at start of current scan period; used to rate-limit on_scan_end()

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

}  // namespace esphome::ln882h_ble_tracker

#endif  // USE_LIBRETINY
