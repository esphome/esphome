#pragma once

#include "esphome/core/defines.h"  // Must be included before conditional includes

#ifdef USE_RP2040_BLE

#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

#include <btstack.h>

#include <cstdint>

namespace esphome::rp2040_ble {

enum class BLEComponentState : uint8_t {
  STATE_OFF = 0,
  ENABLING,
  ACTIVE,
  DISABLING,
  DISABLED,
};

/// One advertisement report from the controller.
struct BLEScanReport {
  uint8_t mac[MAC_ADDRESS_SIZE];  // LSB-first, as the controller delivers it
  int8_t rssi;                    // signed dBm
  uint8_t addr_type;
  uint8_t adv_event_type;  // GAP advertising event type (ADV_IND .. SCAN_RSP); lets a merger tell the two apart
  uint8_t data_len;        // bytes valid in data[]
  // Legacy advertisement (31) + scan response (31). BTstack delivers the two
  // as separate reports, so each report fills at most 31 bytes today; the 62
  // matches the API raw-advertisement contract. adv_event_type is what lets a
  // future merge point tell the two frames apart — carrying it beyond this
  // struct (RawAdvertisement) is deferred until a consumer needs the merge.
  uint8_t data[62];

  // EventPool contract: nothing is heap-allocated inside a report.
  void release() {}
};

/// Consumer interface for controller scan reports. on_scan_report() always
/// runs on the ESPHome main loop: reports are queued from the BTstack packet
/// handler (CYW43 async-context IRQ) and drained by the controller's loop(),
/// so consumers never deal with cross-context state (the esp32_ble
/// event-queue pattern).
class BLEScanListener {
 public:
  virtual void on_scan_report(const BLEScanReport &report) = 0;

 protected:
  ~BLEScanListener() = default;  // deletion via this interface is not part of the contract
};

// Maximum reports buffered between the packet handler and loop(). The producer
// is a same-core IRQ and loop() drains the ring every iteration, so only the
// advertisements of a single loop period can accumulate.
static constexpr uint8_t MAX_SCAN_REPORT_QUEUE_SIZE = 32;

class RP2040BLE final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void enable();
  void disable();
  bool is_active() const { return this->state_ == BLEComponentState::ACTIVE; }

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

  /// Controller BLE address in printable (MSB-first) order, as
  /// gap_local_bd_addr() delivers it — note BLEScanReport::mac is the opposite
  /// (LSB-first) order, hence the explicit names. All zeros until the stack
  /// reports ACTIVE (BTstack reads the address from the controller during
  /// power-up).
  void get_mac_msb_first(uint8_t out[MAC_ADDRESS_SIZE]) const;

#ifdef RP2040_BLE_SCAN_LISTENER_COUNT
  /// Register a consumer for scan reports (delivered on the main loop via loop()).
  /// Storage is codegen-sized: the consumer's codegen requests a slot via
  /// request_scan_listener_slot(), which emits RP2040_BLE_SCAN_LISTENER_COUNT.
  void register_scan_listener(BLEScanListener *listener) { this->scan_listeners_.push_back(listener); }
#endif

  /// Start a controller scan; active sends scan requests and receives scan
  /// responses as separate reports. Interval/window are in BLE units
  /// (0.625 ms). Returns false until the stack is ACTIVE (callers retry — the
  /// tracker's rate-limited retry loop); powering the stack on stays with the
  /// user (enable_on_boot or an explicit enable() call). The controller keeps
  /// no scan state across power cycles: a disable()/enable() cycle ends the
  /// scan, and the caller must call scan_start() again once the stack is back
  /// to ACTIVE (the tracker's loop() reconciliation does exactly that).
  /// While a GATT connect attempt has the scan inhibited, the desired scan is
  /// remembered and started physically when the inhibit is released.
  bool scan_start(uint16_t interval, uint16_t window, bool active);
  /// Stop the controller scan (no-op when not scanning).
  void scan_stop();

#ifdef USE_BLE_GATT_CLIENT
  /// Pause the physical scan for the duration of a GATT connect attempt
  /// (initiating and scanning contend for the radio). The desired scan state
  /// set through scan_start()/scan_stop() is remembered and reconciled by
  /// release_scan_inhibit(). Holders must guarantee the release on every
  /// abort path (the GATT engine reclaims via its connect timeout).
  void inhibit_scan();
  void release_scan_inhibit();
#endif

 protected:
  static void packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size);

  /// Buffer one controller report (BTstack packet handler, CYW43 async-context
  /// IRQ — bounded copy into the lock-free queue, nothing else).
  void enqueue_scan_report_(const uint8_t *mac_lsb_first, int8_t rssi, uint8_t addr_type, uint8_t adv_event_type,
                            const uint8_t *data, uint16_t data_len);

#ifdef RP2040_BLE_SCAN_LISTENER_COUNT
  // Codegen-sized: no heap allocation, no std::vector template instantiation —
  // the same StaticVector pattern as the tracker's ble_device_base listeners.
  StaticVector<BLEScanListener *, RP2040_BLE_SCAN_LISTENER_COUNT> scan_listeners_;
#endif
  // Report ring: the BTstack packet handler (async-context IRQ) allocates a
  // report from the pool, fills it and pushes the pointer; loop() pops,
  // dispatches and releases. Lock-free SPSC — the esp32_ble/bk72xx_ble pattern.
  esphome::LockFreeQueue<BLEScanReport, MAX_SCAN_REPORT_QUEUE_SIZE> report_queue_;
  // Pool sized to queue capacity (SIZE-1): the ring reserves one slot, so
  // allocate() returns nullptr before push() can fail. This prevents leaking a
  // pool slot on a failed push and keeps release() off the producer path.
  esphome::EventPool<BLEScanReport, MAX_SCAN_REPORT_QUEUE_SIZE - 1> report_pool_;

  btstack_packet_callback_registration_t hci_event_callback_registration_{};
  btstack_packet_callback_registration_t sm_event_callback_registration_{};

  uint8_t ble_mac_[MAC_ADDRESS_SIZE]{0};  // printable (MSB-first) order; zeros until ACTIVE
  BLEComponentState state_{BLEComponentState::STATE_OFF};
  bool enable_on_boot_{true};
  bool btstack_initialized_{false};
  bool active_logged_{false};
#ifdef USE_BLE_GATT_CLIENT
  // Remembered scan intent, so connect attempts can pause the physical scan
  // and restore it afterwards without involving the tracker. Counted so
  // overlapping connect attempts compose once multiple slots exist.
  uint16_t scan_interval_{0};
  uint16_t scan_window_{0};
  uint8_t scan_inhibit_count_{0};
  bool scan_active_mode_{false};
  bool scan_desired_{false};
#endif
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern RP2040BLE *global_ble;

}  // namespace esphome::rp2040_ble

#endif  // USE_RP2040_BLE
