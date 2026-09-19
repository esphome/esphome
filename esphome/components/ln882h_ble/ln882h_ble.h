#pragma once

#include "esphome/core/defines.h"

#ifdef USE_LN882H_BLE

#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

#include <atomic>
#include <cstdint>

namespace esphome::ln882h_ble {

enum class BLEComponentState : uint8_t {
  STATE_OFF = 0,
  ENABLING,
  ACTIVE,
};

/// One scan report from the controller, decoded from the SDK's rw-task event
/// (RSSI already sign-corrected).
struct BLEScanReport {
  uint8_t mac[MAC_ADDRESS_SIZE];  // as the controller delivers it (LSB-first)
  int8_t rssi;                    // signed dBm (-127..+20)
  uint8_t addr_type;
  bool is_scan_response;  // report is a scan response (active scan)
  bool scannable;         // advertisement may be followed by a scan response
  uint8_t data_len;       // bytes valid in data[] (<= 62)
  // Each report carries ONE frame — a legacy advertisement (<=31 B) or a scan
  // response (<=31 B) — delivered split, exactly as the SDK reports them. The
  // TRACKER merges the pair into a single frame before any consumer sees it
  // (Bluedroid semantics, HubCapabilities::merges_scan_response). 62 is twice
  // the legacy maximum: defensive headroom for the data_len clamp, and the
  // same width as the merged framing downstream.
  uint8_t data[62];

  // EventPool contract: nothing is heap-allocated inside a report.
  void release() {}
};

/// Consumer interface for controller scan reports. on_scan_report() always runs
/// on the ESPHome main task: reports are queued from the SDK's rw task and
/// drained by the controller's loop(), so consumers never deal with cross-task
/// state (the esp32_ble event-queue pattern).
class BLEScanListener {
 public:
  virtual void on_scan_report(const BLEScanReport &report) = 0;

 protected:
  ~BLEScanListener() = default;  // deletion via this interface is not part of the contract
};

// Maximum reports buffered between the rw task and loop(). Sized from the
// measured worst case, not copied: WiFi/BLE coexistence delays rw-task report
// delivery by up to ~136 ms on this device (see the tracker's pending-adv
// timeout rationale), and a busy 2.4 GHz environment delivers ~200-400
// reports/s — a stall plus one loop() interval buffers ~30-60 reports, so 63
// usable slots absorb it with margin. ~4.7 KB at high water, reached only
// during such stalls.
static constexpr uint8_t MAX_SCAN_REPORT_QUEUE_SIZE = 64;

// Rejected frames tolerated before the first delivered report without
// declaring the scanner dead (boot-time stray extended frames are normal).
static constexpr uint16_t REJECTED_DEAD_SCANNER_THRESHOLD = 16;

class LN882HBLE final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  /// Bring up the LN882H BLE stack (one-time; the SDK has no teardown path).
  /// Requires setup() to have run: rw_init() is handed the address resolved
  /// there. Blocks ~120 ms across the SDK's settle points, so calling it from
  /// loop() (enable_on_boot: false) trips the loop-blocking warning once.
  void enable();
  bool is_active() const { return this->state_ == BLEComponentState::ACTIVE; }

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

  /// Controller BLE address in the SDK's ln_bd_addr_t order: least-significant
  /// octet first (the BLE/HCI convention). Reverse for printable form — the
  /// byte order is in the name so platform analogs cannot be confused
  /// (the bk72xx sibling exposes the same accessor).
  void get_mac_lsb_first(uint8_t out[6]) const;

#ifdef LN882H_BLE_SCAN_LISTENER_COUNT
  /// Register a consumer for scan reports (delivered on the main task via loop()).
  /// Storage is codegen-sized: the consumer's codegen requests a slot via
  /// request_scan_listener_slot(), which emits LN882H_BLE_SCAN_LISTENER_COUNT.
  void register_scan_listener(BLEScanListener *listener) { this->scan_listeners_.push_back(listener); }
#endif

  /// Start the controller scan. Interval/window are in BLE units (0.625 ms);
  /// active enables scan requests on the 1M PHY. Enables the stack first if
  /// needed. Scans the legacy 1M PHY only (extended/coded PHY advertisements
  /// exceed the legacy 62-byte framing consumers are sized for).
  void scan_start(uint16_t interval, uint16_t window, bool active);
  /// Stop the controller scan (no-op when not scanning).
  void scan_stop();

  /// Internal, SDK rw-task event-callback context: allocate a pool slot for a
  /// scan report. Returns nullptr (and counts the drop) when the queue is full;
  /// the callback fills the slot in place — no intermediate copy.
  BLEScanReport *allocate_scan_report();
  /// Internal: hand a filled slot to the main-task queue (cannot fail — the
  /// pool is sized to the queue capacity).
  void push_scan_report(BLEScanReport *report);
  /// Internal, rw-task context: count a report rejected by the legacy-only
  /// filter, so a wrong assumption about the stack's report encoding shows up
  /// in verbose logs instead of as a scanner that silently reports nothing.
  void count_rejected_report() { this->rejected_reports_.fetch_add(1, std::memory_order_relaxed); }

 protected:
  void resolve_mac_();

#ifdef LN882H_BLE_SCAN_LISTENER_COUNT
  // Codegen-sized: no heap allocation, no std::vector template instantiation —
  // the same StaticVector pattern as the tracker's ble_device_base listeners.
  StaticVector<BLEScanListener *, LN882H_BLE_SCAN_LISTENER_COUNT> scan_listeners_;
#endif
  // Report ring: the SDK event callback (rw task) allocates a report from the
  // pool, fills it and pushes the pointer; loop() pops, dispatches and releases.
  // Lock-free SPSC, zero allocation at steady state — the esp32_ble pattern.
  // Overflow drops the NEWEST report (allocate fails, producer counts and
  // returns) — under a coexistence stall the freshest advertisements are lost
  // while queued ones drain. Deliberate: matches esp32_ble, and dropping from
  // the head would need consumer-side locking this design exists to avoid.
  esphome::LockFreeQueue<BLEScanReport, MAX_SCAN_REPORT_QUEUE_SIZE> report_queue_;
  // Pool sized to queue capacity (SIZE-1): the ring reserves one slot, so
  // allocate() returns nullptr before push() can fail. This prevents leaking a
  // pool slot on a failed push and keeps release() off the producer path.
  esphome::EventPool<BLEScanReport, MAX_SCAN_REPORT_QUEUE_SIZE - 1> report_pool_;
  // Reports rejected by the legacy-only filter (rw-task producer, main-task
  // consumer via exchange in loop()).
  std::atomic<uint16_t> rejected_reports_{0};
  uint8_t ble_mac_[MAC_ADDRESS_SIZE]{0};  // controller (LSB-first) order, as ln_bd_addr_t stores it
  BLEComponentState state_{BLEComponentState::STATE_OFF};
  bool enable_on_boot_{false};
  bool scanning_{false};  // controller scan running (re-entry guard for scan_start)
  // Dead-scanner diagnosis: done once a report is delivered or the one-shot
  // warning has fired, whichever comes first.
  bool reject_diagnosis_done_{false};
  uint32_t rejected_before_delivery_{0};  // drives the dead-scanner warning
};

}  // namespace esphome::ln882h_ble

#endif  // USE_LN882H_BLE
