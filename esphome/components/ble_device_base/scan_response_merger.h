// Shared support for trackers whose controller delivers advertisement and
// scan response as SEPARATE reports (ln882h, rp2, bk72xx; ESP-IDF concatenates
// both into one result before ESPHome sees it):
//
//   ScanResponseMerger — Bluedroid-style merge: a scannable advertisement is
//   held briefly, its scan response is appended on arrival and the pair is
//   delivered as ONE merged frame. Merged delivery is what the receiving side
//   is built around: Home Assistant keeps the latest raw frame per device and
//   skips re-parsing when it is unchanged — split delivery alternates two raw
//   frames per device and defeats both.
//
//   AdvDispatcher — the delivery half every such tracker repeats: raw
//   callback, listener parsing, discovered-device log. Trackers delegate
//   their BLEHub register_listener / set_raw_advertisement_callback here.
//
// The merger delivers straight into the tracker's AdvDispatcher — bind() wires
// the pair once in setup(). Single-task use only (every tracker calls this on
// the ESPHome main task). The clock is caller-provided: pass the same clock to
// stash_adv() and sweep() (millis() or App.get_loop_component_start_time(),
// never mixed).

#pragma once

#include "esphome/core/defines.h"

// Emitted (cg.add_define) by each tracker that adopts the merger, so builds
// whose tracker merges in-stack (esp32) never compile this code.
#ifdef USE_BLE_SCAN_RESPONSE_MERGER

#include "ble_device.h"
#include "ble_hub.h"
#include "esphome/core/helpers.h"

#include <cstdint>

namespace esphome::ble_device_base {

/// The delivery half of a split-report tracker, shared so the dispatch
/// contract (raw-callback ordering, raw_only gate, discovered-log policy)
/// lives in one place. Owns the members every tracker otherwise duplicates;
/// the tracker's BLEHub methods delegate here.
class AdvDispatcher {
 public:
  void register_listener(ESPBTDeviceListener *listener) {
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
    this->listeners_.push_back(listener);
#endif
  }
  void set_raw_advertisement_callback(RawAdvertisementCallback callback) { this->raw_callback_ = callback; }
  /// Dispatch one (possibly merged) advertisement: the raw callback, and —
  /// unless raw_only — parsing for listeners/triggers. raw_only marks
  /// unmatched scan-response frames: forwarded on the raw callback only, never
  /// parsed for local sensors/triggers (Home Assistant merges per address).
  /// log_unclaimed_tag: when non-null, a device no listener claimed is logged
  /// under this tag (esp32_ble_tracker parity: pass the tracker TAG on
  /// one-shot scans, nullptr on continuous scans, which would spam).
  void dispatch(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data, uint8_t data_len,
                bool raw_only, const char *log_unclaimed_tag);
  /// Fire listeners' on_scan_end and reset the per-scan discovered-log dedup.
  void on_scan_end();

 protected:
  RawAdvertisementCallback raw_callback_{};
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Parsed-advertisement consumers registered through ble_device_base.
  // Codegen-sized: no heap allocation, no std::vector template instantiations.
  StaticVector<ESPBTDeviceListener *, ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT> listeners_;
  // Per-period "Found device" DEBUG log with MAC dedup. Guarded like its only
  // writer so a no-listener build does not carry an unused vector.
  DiscoveredDeviceLog discovered_log_{};
#endif
};

class ScanResponseMerger {
 public:
  /// Wire the merger's output; call once in the tracker's setup(). Every
  /// delivered frame goes to dispatcher->dispatch(); scan_continuous is read
  /// at each delivery (runtime continuous flips are honored) to decide the
  /// unclaimed-device log tag, so both pointers must outlive the merger —
  /// tracker members always do.
  void bind(AdvDispatcher *dispatcher, const bool *scan_continuous, const char *log_tag) {
    this->dispatcher_ = dispatcher;
    this->scan_continuous_ = scan_continuous;
    this->log_tag_ = log_tag;
  }
  /// Hold a scannable advertisement, waiting for its scan response. The
  /// tracker calls this only when it wants the merge (scannable advertisement
  /// while an active scan runs) and delivers everything else directly. A
  /// same-device re-advertisement delivers the held frame (its scan response
  /// is not coming) and reuses the slot; a full table degrades gracefully to
  /// unmerged delivery.
  void stash_adv(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data, uint8_t data_len,
                 uint32_t now);
  /// A scan response arrived: append it to the held advertisement from the
  /// same device and deliver the pair as one frame. The merged frame reports
  /// the ADVERTISEMENT's RSSI — every unmerged path reports the
  /// advertisement's measurement, so a device's RSSI must not jump between two
  /// measurements depending on merge timing. Unmatched responses are delivered
  /// raw_only.
  void submit_scan_rsp(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data, uint8_t data_len);
  /// Timeout flush (call from loop() with the stash_adv() clock): deliver
  /// held advertisements whose scan response never arrived (device didn't
  /// answer / frame lost) — unmerged, past PENDING_ADV_TIMEOUT_MS.
  void sweep(uint32_t now);
  /// Deliver every held advertisement now (scan period/scan is ending, before
  /// on_scan_end fires): unmerged delivery, same as the timeout path.
  void flush();
  /// Lets loop() skip the cross-TU sweep() call in the common case (empty:
  /// passive scan, or every pair already matched).
  bool empty() const { return this->pending_count_ == 0; }

 private:
  /// All delivery funnels through here: an unbound merger (bind() not called)
  /// drops the frame instead of jumping through a null pointer, mirroring the
  /// guard-before-invoke convention of the ble_hub.h callback slots.
  void deliver_(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data, uint8_t data_len,
                bool raw_only);

  // 62 bytes = legacy adv (31) + scan response (31), the same merged maximum
  // as ESP-IDF delivers on ESP32.
  struct PendingAdv {
    bool used{false};
    uint8_t mac[MAC_ADDRESS_SIZE];
    uint8_t addr_type;
    int8_t rssi;
    uint8_t data_len;  // <= sizeof(data)
    uint8_t data[62];
    uint32_t stored_ms;
  };
  // Sized for the unanswered case: a pair that IS answered normally matches
  // within one report-queue drain, so a slot is held for the full timeout only
  // by scannable devices that never reply. 8 concurrent such advertisers
  // before the merge degrades (frames still delivered, just unmerged) at
  // ~80 B each.
  static constexpr size_t MAX_PENDING_ADV = 8;
  // On air a scan response follows its advertisement by T_IFS (150 µs) — the
  // timeout only covers HOST-side report queuing under WiFi/BLE coexistence,
  // measured on-device (ln882h) at up to ~136 ms. 300 ms = >2x that margin,
  // while staying below any device's re-advertising period.
  static constexpr uint32_t PENDING_ADV_TIMEOUT_MS = 300;
  AdvDispatcher *dispatcher_{nullptr};
  const bool *scan_continuous_{nullptr};  // read at delivery; see bind()
  const char *log_tag_{nullptr};
  // pending_count_ mirrors the number of set `used` flags; both are updated
  // together on every transition.
  PendingAdv pending_adv_[MAX_PENDING_ADV];
  uint8_t pending_count_{0};
};

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_SCAN_RESPONSE_MERGER
