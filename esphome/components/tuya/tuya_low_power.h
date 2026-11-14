#pragma once

// MCU transaction bookkeeping, testable on the host without ESPHome or a platform.
#include <cstdint>

namespace esphome::tuya {

/// Tracks whether the MCU's state is known and whether it can take a write.
///
/// A version-0 MCU reports its state as a series of CMD05 frames, after the module said it is online and after
/// a write. The series has no end marker, so it counts as complete once the MCU has been quiet for QUIET_MS.
/// A version-3 MCU sends its state in one CMD07 report; mark_synchronized() is called for it.
class TuyaLowPowerSession {
 public:
  static constexpr uint32_t QUIET_MS = 500;
  static constexpr uint32_t REPORT_TIMEOUT_MS = 12000;

  enum class Event : uint8_t { EVENT_NONE, EVENT_SYNCHRONIZED, EVENT_TIMED_OUT };

  void product_received() { this->product_ = true; }
  bool has_product() const { return this->product_; }
  bool synchronized() const { return this->synced_; }
  bool faulted() const { return this->fault_; }
  bool busy() const { return this->status_pending_ || this->write_pending_; }
  bool ready() const { return this->product_ && this->synced_ && !this->fault_ && !this->busy(); }

  /// The MCU or the network changed: the known state is no longer current.
  void reset_status() {
    this->synced_ = false;
    this->status_pending_ = false;
    this->write_pending_ = false;
  }

  /// The module asked for the full state (network online); its CMD05 series follows.
  void begin_full_status(uint32_t now) {
    this->synced_ = false;
    this->status_pending_ = true;
    this->reported_ = false;
    this->started_ms_ = now;
  }

  /// A write (CMD09) was sent. It ends after the MCU's ACK and the reports that follow it.
  void begin_write(uint32_t now) {
    this->write_pending_ = true;
    this->acked_ = false;
    this->started_ms_ = now;
  }

  void write_ack(uint32_t now) {
    if (!this->write_pending_)
      return;
    this->acked_ = true;
    this->last_report_ms_ = now;
  }

  /// A datapoint report (CMD05) arrived.
  void observe_report(uint32_t now) {
    this->reported_ = true;
    this->last_report_ms_ = now;
  }

  /// Ends a status series or write after QUIET_MS without reports, or with a fault after REPORT_TIMEOUT_MS.
  Event update(uint32_t now) {
    if (this->status_pending_) {
      if (this->reported_ && now - this->last_report_ms_ >= QUIET_MS) {
        this->status_pending_ = false;
        this->synced_ = true;
        this->fault_ = false;
        return Event::EVENT_SYNCHRONIZED;
      }
      if (now - this->started_ms_ >= REPORT_TIMEOUT_MS) {
        this->command_timeout();
        return Event::EVENT_TIMED_OUT;
      }
    }
    if (this->write_pending_) {
      if (this->acked_ && now - this->last_report_ms_ >= QUIET_MS) {
        this->write_pending_ = false;
      } else if (now - this->started_ms_ >= REPORT_TIMEOUT_MS) {
        this->command_timeout();
        return Event::EVENT_TIMED_OUT;
      }
    }
    return Event::EVENT_NONE;
  }

  /// No answer to a command: the state is unknown until the next full status.
  void command_timeout() {
    this->synced_ = false;
    this->fault_ = true;
    this->status_pending_ = false;
    this->write_pending_ = false;
  }

  /// A version-3 MCU sent its complete state in one report.
  void mark_synchronized() {
    this->status_pending_ = false;
    this->synced_ = true;
    this->fault_ = false;
  }

 protected:
  uint32_t started_ms_{0};
  uint32_t last_report_ms_{0};
  bool product_{false};
  bool synced_{false};
  bool fault_{false};
  bool status_pending_{false};
  bool reported_{false};
  bool write_pending_{false};
  bool acked_{false};
};

}  // namespace esphome::tuya
