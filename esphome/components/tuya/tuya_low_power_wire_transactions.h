#pragma once

// The one MCU command (CMD01, CMD02, CMD06, CMD08 or CMD09) waiting for its answer. The protocol has no sequence
// numbers, so a late answer cannot be matched to the command it belongs to: after a timeout no command is sent until
// the MCU answered a product query again.
#include <cstddef>
#include <cstdint>

namespace esphome::tuya {

class TuyaLowPowerWireTransactions {
 public:
  static constexpr uint32_t ACK_TIMEOUT_MS = 1500;

  bool begin(uint8_t command, uint32_t now) {
    if (this->active_ || this->faulted_ ||
        (command != 0x01 && command != 0x02 && command != 0x06 && command != 0x08 && command != 0x09))
      return false;
    this->command_ = command;
    this->active_ = true;
    this->started_ms_ = now;
    return true;
  }

  /// Whether this frame answers the waiting command: the product info, an empty CMD09 ACK, a CMD02 answer
  /// (empty, or the two pin bytes of a version-3 MCU), or the CMD07 report that answers a version-3 write (CMD06) or
  /// status query (CMD08).
  bool accepts(uint8_t command, size_t len) const {
    if (!this->active_)
      return false;
    if (this->command_ == 0x06 || this->command_ == 0x08)
      return command == 0x07 && len >= 4;
    if (command == 0x02)
      return this->command_ == 0x02 && (len == 0 || len == 2);
    return command == this->command_ && (command == 0x01) == (len != 0);
  }

  bool complete(uint8_t command, size_t len) {
    if (!this->accepts(command, len))
      return false;
    this->active_ = false;
    return true;
  }

  bool overdue(uint32_t now) const { return this->active_ && now - this->started_ms_ >= ACK_TIMEOUT_MS; }

  bool expire(uint32_t now) {
    if (!this->overdue(now))
      return false;
    this->transport_fault();
    return true;
  }

  /// A version-3 write or query without its CMD07 report: a late report carries its own values, so no quarantine.
  bool abandon_report(uint32_t now) {
    if (!this->overdue(now) || (this->command_ != 0x06 && this->command_ != 0x08))
      return false;
    this->active_ = false;
    return true;
  }
  void transport_fault() {
    this->active_ = false;
    this->faulted_ = true;
  }

  /// The MCU answered a product query again.
  void recovered() {
    if (!this->active_)
      this->faulted_ = false;
  }

  bool active() const { return this->active_; }
  bool faulted() const { return this->faulted_; }
  bool idle() const { return !this->active_ && !this->faulted_; }
  /// The waiting command, valid while active().
  uint8_t command() const { return this->command_; }

 protected:
  uint32_t started_ms_{0};
  uint8_t command_{0};
  bool active_{false};
  bool faulted_{false};
};

}  // namespace esphome::tuya
