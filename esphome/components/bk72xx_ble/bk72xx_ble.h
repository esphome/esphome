#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BK72XX_BLE

#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

#include <cstdint>

#include "bdk_scan.h"

namespace esphome::bk72xx_ble {

enum class BLEComponentState : uint8_t {
  STATE_OFF = 0,
  ENABLING,
  ACTIVE,
};

/// Outcome of one reconciliation step.
enum class ScanOpResult : uint8_t {
  SETTLED,  ///< The request is reached: scan observed running, or stopped
            ///< with the activity fully released.
  PENDING,  ///< A step is in flight; loop() keeps advancing — call
            ///< scan_start() again to learn the outcome.
  FAILED,   ///< The controller rejected a step; retry later.
};

/// One scan request: mode plus timing, in BLE units (0.625 ms).
struct ScanParams {
  bool active;
  uint16_t interval;
  uint16_t window;
  bool operator==(const ScanParams &) const = default;
};

/// One advertisement report from the controller.
struct BLEScanReport {
  uint8_t mac[MAC_ADDRESS_SIZE];  // LSB-first, as the controller delivers it
  int8_t rssi;                    // signed dBm
  uint8_t addr_type;
  // GAPM report info byte (recv_adv_t.evt_type): bits 0-2 report type
  // (1 = legacy adv, 3 = legacy scan response), bit 5 scannable — lets the
  // tracker's merger tell the two frames apart.
  uint8_t evt_type;
  uint8_t data_len;  // bytes valid in data[]
  uint8_t data[62];  // legacy advertisement (31) + scan response (31)

  // EventPool contract: nothing is heap-allocated inside a report.
  void release() {}
};

/// Consumer interface for controller scan reports. on_scan_report() always runs
/// on the ESPHome main task: reports are queued from the BDK BLE task and
/// drained by the controller's loop(), so consumers never deal with cross-task
/// state (the esp32_ble event-queue pattern).
class BLEScanListener {
 public:
  virtual void on_scan_report(const BLEScanReport &report) = 0;

 protected:
  ~BLEScanListener() = default;  // deletion via this interface is not part of the contract
};

// Maximum reports buffered between the BLE task and loop().
static constexpr uint8_t MAX_SCAN_REPORT_QUEUE_SIZE = 64;

class BK72xxBLE final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  /// Bring up the BDK BLE stack (one-time; the BDK has no teardown path).
  void enable();
  bool is_active() const { return this->state_ == BLEComponentState::ACTIVE; }

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

  /// Controller BLE address, least-significant octet first (BLE convention).
  void get_mac_lsb_first(uint8_t out[MAC_ADDRESS_SIZE]) const;

#ifdef BK72XX_BLE_SCAN_LISTENER_COUNT
  /// Register a consumer for scan reports (delivered on the main task via loop()).
  /// Storage is codegen-sized: the consumer's codegen requests a slot via
  /// request_scan_listener_slot(), which emits BK72XX_BLE_SCAN_LISTENER_COUNT.
  void register_scan_listener(BLEScanListener *listener) { this->scan_listeners_.push_back(listener); }
#endif

  /// Request a scan (interval/window in 0.625 ms BLE units); enables the
  /// stack first if needed. PENDING until the scan is observed running —
  /// loop() keeps advancing, call again to learn the outcome.
  ScanOpResult scan_start(uint16_t interval, uint16_t window, bool active);
  /// Request the scanner stopped and the activity released; steps that
  /// cannot run yet are completed from loop().
  void scan_stop();
  /// Drive a requested stop until the radio is observed idle, bounded by
  /// timeout_ms (for OTA). Returns false if it still has not settled.
  bool flush_pending_stop(uint32_t timeout_ms);
  /// Last reconciliation outcome; on FAILED the consumer's retry policy owns
  /// recovery.
  ScanOpResult last_scan_result() const { return this->last_result_; }

  /// Internal: buffer one controller report (BDK notice callback, BLE task
  /// context — bounded copy under the scheduler lock, nothing else).
  void enqueue_scan_report(const uint8_t *mac, int8_t rssi, uint8_t addr_type, uint8_t evt_type, const uint8_t *data,
                           uint16_t data_len);

 protected:
  void resolve_mac_();
  ScanOpResult advance_();
  ScanOpResult advance_stop_(BdkActivityState state, bool ready);
  ScanOpResult advance_start_(BdkActivityState state, bool ready);
  bool teardown_stuck_(uint32_t now);
  void reset_teardown_episode_();
  void release_activity_(BdkActivityState state);

#ifdef BK72XX_BLE_SCAN_LISTENER_COUNT
  // Codegen-sized: no heap allocation, no std::vector template instantiation —
  // the same StaticVector pattern as the tracker's ble_device_base listeners.
  StaticVector<BLEScanListener *, BK72XX_BLE_SCAN_LISTENER_COUNT> scan_listeners_;
#endif
  // Report ring: the BDK notice callback (BLE task) allocates a report from the
  // pool, fills it and pushes the pointer; loop() pops, dispatches and releases.
  // Lock-free SPSC, zero allocation at steady state — the esp32_ble pattern.
  esphome::LockFreeQueue<BLEScanReport, MAX_SCAN_REPORT_QUEUE_SIZE> report_queue_;
  // Pool sized to queue capacity (SIZE-1): the ring reserves one slot, so
  // allocate() returns nullptr before push() can fail. This prevents leaking a
  // pool slot on a failed push and keeps release() off the producer path.
  esphome::EventPool<BLEScanReport, MAX_SCAN_REPORT_QUEUE_SIZE - 1> report_pool_;
  // Largest-to-smallest: padding only at the tail, absorbed by future byte fields.
  uint32_t last_advance_ms_{0};
  uint32_t pending_since_ms_{0};          // bring-up budget anchor; refilled on request change
  uint32_t teardown_since_ms_{0};         // unfinished teardown episode start; 0 = none
  uint32_t teardown_stuck_log_ms_{0};     // last stuck-teardown ERROR; re-logged each TEARDOWN_STUCK_ERROR_MS
  int last_release_err_{0};               // SDK code of the episode's last failed release; 0 = none
  ScanParams requested_{};                // latched by scan_start()
  ScanParams applied_{};                  // last params we commanded; mismatch with requested_ restarts
  uint8_t ble_mac_[MAC_ADDRESS_SIZE]{0};  // LSB-first (BLE convention)
  uint8_t scan_activity_idx_{INVALID_ACTIVITY_IDX};
  bool scan_wanted_{false};     // the latched request is to scan (vs stopped)
  bool release_warned_{false};  // gates the release WARN; widens the pump gate
  bool restarting_{false};      // mode-change release in flight; teardown deadline governs until released
  bool enable_on_boot_{false};
  // PENDING means advance_() has more to do; loop() drives it, paced and
  // (for a bring-up) bounded.
  ScanOpResult last_result_{ScanOpResult::SETTLED};
  BLEComponentState state_{BLEComponentState::STATE_OFF};
};

}  // namespace esphome::bk72xx_ble

#endif  // USE_BK72XX_BLE
