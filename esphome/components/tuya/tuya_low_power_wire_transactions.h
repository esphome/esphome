#pragma once

// The one MCU command (CMD01, CMD02 or CMD09) waiting for its answer. The protocol has no sequence numbers, so a
// late answer cannot be matched to the command it belongs to: after a timeout no command is sent until the MCU
// answered a product query again.
#include <cstddef>
#include <cstdint>

namespace esphome::tuya {

class TuyaLowPowerWireTransactions {
 public:
  static constexpr uint32_t ACK_TIMEOUT_MS = 1500;

  bool begin(uint8_t command, uint32_t now) {
    if (this->active_ || this->faulted_ || (command != 0x01 && command != 0x02 && command != 0x09))
      return false;
    this->command_ = command;
    this->active_ = true;
    this->started_ms_ = now;
    return true;
  }

  /// Whether this frame answers the waiting command: the product info, or an empty CMD02/CMD09 ACK.
  bool accepts(uint8_t command, size_t len) const {
    return this->active_ && command == this->command_ && (command == 0x01) == (len != 0);
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
