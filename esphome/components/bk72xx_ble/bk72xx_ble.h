#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BK72XX_BLE

#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

#include <cstdint>

namespace esphome::bk72xx_ble {

enum class BLEComponentState : uint8_t {
  STATE_OFF = 0,
  ENABLING,
  ACTIVE,
};

/// Outcome of a scan_start() call.
enum class ScanStartResult : uint8_t {
  STARTED,  ///< The controller accepted the start; the scan chain is running.
  PENDING,  ///< The start sequence is still in progress (an asynchronous
            ///< activity create, or another BLE operation in flight); call
            ///< scan_start() again to advance it.
  FAILED,   ///< The controller rejected the start; retry later.
};

/// scan_actv_idx_ value marking "no scan activity", the BDK's own convention.
static constexpr uint8_t INVALID_ACTV_IDX = 0xFF;

/// One advertisement report from the controller.
struct BLEScanReport {
  uint8_t mac[6];  // LSB-first, as the controller delivers it
  int8_t rssi;     // signed dBm
  uint8_t addr_type;
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
  void get_mac_lsb_first(uint8_t out[6]) const;

#ifdef BK72XX_BLE_SCAN_LISTENER_COUNT
  /// Register a consumer for scan reports (delivered on the main task via loop()).
  /// Storage is codegen-sized: the consumer's codegen requests a slot via
  /// request_scan_listener_slot(), which emits BK72XX_BLE_SCAN_LISTENER_COUNT.
  void register_scan_listener(BLEScanListener *listener) { this->scan_listeners_.push_back(listener); }
#endif

  /// Start the controller scan. Interval/window are in BLE units (0.625 ms);
  /// enables the stack first if needed. An active start involves an
  /// asynchronous activity create, reported as PENDING — call again to
  /// advance the sequence until it reports STARTED or FAILED.
  ScanStartResult scan_start(uint16_t interval, uint16_t window, bool active);
  /// Stop the controller scan (no-op when not scanning). Also discards a scan
  /// activity that was created for an active scan but not started yet.
  void scan_stop();

  /// Internal: buffer one controller report (BDK notice callback, BLE task
  /// context — bounded copy under the scheduler lock, nothing else).
  void enqueue_scan_report(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data, uint16_t data_len);

 protected:
  void resolve_mac_();
  bool acquire_scan_activity_();
  ScanStartResult active_scan_start_(uint16_t interval, uint16_t window);

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
  uint8_t ble_mac_[6]{0};  // LSB-first (BLE convention)
  uint8_t scan_actv_idx_{INVALID_ACTV_IDX};
  BLEComponentState state_{BLEComponentState::STATE_OFF};
  bool enable_on_boot_{false};
};

}  // namespace esphome::bk72xx_ble

#endif  // USE_BK72XX_BLE
