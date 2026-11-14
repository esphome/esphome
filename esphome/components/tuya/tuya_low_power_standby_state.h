#pragma once

#include <cstdint>

namespace esphome::tuya {

// Pure MCU-protocol state. The platform never interprets CMD12/38 or
// the HC32's 'sm' product field. All transitions are host-testable.
class TuyaLowPowerStandbyState {
 public:
  enum class ResumeAction : uint8_t { NONE, SEND_CMD12_WAKE, REQUIRE_RESYNC, KEEP_CONSENT };

  bool accept_request(bool standby_mode, bool mcu_idle, bool uart_healthy, bool other_owners_idle) {
    if (requested_ || notified_ || recovery_required_ || !standby_mode || !mcu_idle || !uart_healthy ||
        !other_owners_idle)
      return false;
    requested_ = true;
    return true;
  }

  // Version-3 MCUs (sm=2) agree to sleep with 0x91: their answer is the whole handshake, and the
  // MCU sleeps right after it, so the consent counts as notified and the module may enter PM at once.
  bool accept_offer(bool mcu_idle, bool uart_healthy, bool other_owners_idle) {
    if (!this->accept_request(true, mcu_idle, uart_healthy, other_owners_idle))
      return false;
    notified_ = true;
    return true;
  }

  // A version-3 MCU sends no boot heartbeat. A fresh status report after a resync ends the recovery.
  void recovered() { recovery_required_ = false; }

  bool requested() const { return requested_; }
  bool notified() const { return notified_; }
  bool recovery_required() const { return recovery_required_; }
  bool needs_notification() const { return requested_ && !notified_; }
  bool ready_for_pm() const { return requested_ && notified_ && !deferred_ && !recovery_required_; }

  // The CMD12/01 frame is sent, fully drained, and the current preparation
  // deliberately returns false so UART/scheduler idle must stabilize again.
  void notification_sent() {
    notified_ = true;
    deferred_ = true;
  }

  // Called when an attempt is declined by the platform. The *first* decline
  // after CMD12/01 is intentional. Any later veto must compensate CMD12/00.
  bool cancelled() {
    if (deferred_) {
      deferred_ = false;
      return false;
    }
    return this->force_cancel();
  }

  // Returns true if a compensating CMD12/00 is necessary. For a failed PM
  // request this is an abort notification, not a GPIO wake notification.
  bool force_cancel() {
    const bool compensate = notified_;
    requested_ = false;
    notified_ = false;
    deferred_ = false;
    return compensate;
  }

  // keep_consent: the module itself woke (timer or received network data) while the MCU
  // still sleeps. The MCU's consent and the sent CMD12/01 stay valid, so the module may sleep
  // again without a new CMD38; any MCU traffic or a write ends the consent as before.
  ResumeAction resumed(bool expected_wake, bool gpio_wake, bool keep_consent = false) {
    if (notified_ && expected_wake && !gpio_wake && keep_consent && !recovery_required_) {
      deferred_ = false;
      return ResumeAction::KEEP_CONSENT;
    }
    const bool had_notification = notified_;
    requested_ = false;
    notified_ = false;
    deferred_ = false;
    if (!had_notification)
      return ResumeAction::NONE;
    if (expected_wake && gpio_wake)
      return ResumeAction::SEND_CMD12_WAKE;
    // Timer-only, unknown/aborted PM or an unexpected source does not prove
    // the original WXU's conditional CMD12/00 GPIO-wake path occurred.
    recovery_required_ = true;
    return ResumeAction::REQUIRE_RESYNC;
  }

  void transport_fault() {
    recovery_required_ = true;
    requested_ = false;
    notified_ = false;
    deferred_ = false;
  }

  // An observed MCU reset invalidates the old sleep transaction. Higher
  // layers must still re-run CMD01 and the full datapoint state sequence.
  void reset_on_mcu_boot() {
    requested_ = false;
    notified_ = false;
    deferred_ = false;
    recovery_required_ = false;
  }

 private:
  bool requested_{false};
  bool notified_{false};
  bool deferred_{false};
  bool recovery_required_{false};
};

}  // namespace esphome::tuya
