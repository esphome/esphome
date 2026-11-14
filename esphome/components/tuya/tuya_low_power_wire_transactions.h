#pragma once

// Protocol-neutral MCU wire-transaction gate for Tuya low_power.
// No sequence numbers exist in the original protocol: serializing writes
// prevents local overlap but cannot prove an arbitrarily late same-CMD ACK
// belongs to the latest transaction. An ambiguous timeout blocks new writes.
#include <cstddef>
#include <cstdint>

namespace esphome::tuya {

class TuyaLowPowerWireTransactions {
 public:
  enum class Kind : uint8_t { NONE, PRODUCT, NETWORK, WRITE };
  static constexpr uint32_t ACK_TIMEOUT_MS = 1500;

  bool begin(uint8_t command, uint32_t now) {
    if (active_ || faulted_)
      return false;
    if (command == 0x01) {
      kind_ = Kind::PRODUCT;
    } else if (command == 0x02) {
      kind_ = Kind::NETWORK;
    } else if (command == 0x09) {
      kind_ = Kind::WRITE;
    } else {
      return false;  // Direct replies/notifications must not claim a queue slot.
    }
    active_ = true;
    started_ms_ = now;
    ++generation_;
    return true;
  }

  bool accepts(uint8_t command, size_t len) const {
    if (!active_)
      return false;
    switch (kind_) {
      case Kind::PRODUCT:
        return command == 0x01 && len != 0;
      case Kind::NETWORK:
        return command == 0x02 && len == 0;
      case Kind::WRITE:
        return command == 0x09 && len == 0;
      default:
        return false;
    }
  }

  bool complete(uint8_t command, size_t len) {
    if (!accepts(command, len))
      return false;
    active_ = false;
    kind_ = Kind::NONE;
    ++generation_;
    return true;
  }

  // Handles ACKs arriving after the deadline but before a timeout sweep.
  // Wrap-safe for elapsed intervals shorter than 2^31 milliseconds.
  bool overdue(uint32_t now) const { return active_ && static_cast<uint32_t>(now - started_ms_) >= ACK_TIMEOUT_MS; }

  bool expire(uint32_t now) {
    if (!this->overdue(now))
      return false;
    active_ = false;
    kind_ = Kind::NONE;
    faulted_ = true;
    ++generation_;
    return true;
  }

  void transport_fault() {
    active_ = false;
    kind_ = Kind::NONE;
    faulted_ = true;
    ++generation_;
  }

  // Explicit full MCU status/identity recovery, not a CMD09 ACK.
  void recovered() {
    if (active_)
      return;
    faulted_ = false;
    ++generation_;
  }

  bool active() const { return active_; }
  bool faulted() const { return faulted_; }
  bool idle() const { return !active_ && !faulted_; }
  Kind kind() const { return kind_; }
  uint32_t generation() const { return generation_; }

 private:
  Kind kind_{Kind::NONE};
  uint32_t started_ms_{0};
  uint32_t generation_{0};
  bool active_{false};
  bool faulted_{false};
};

}  // namespace esphome::tuya
