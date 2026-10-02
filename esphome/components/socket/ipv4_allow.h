#pragma once

#include <cstdint>

namespace esphome::socket {

/// IPv4 addresses that may connect. An empty list allows every address.
/// A single address is stored as /32. Addresses and masks are host byte order.
///
/// tcp_uart's server role is the first caller. uart_tcp uses the same list
/// once its server role is on dev, which is why this lives next to the socket
/// helpers instead of inside the first caller.
class Ipv4Allow {
 public:
  static constexpr uint8_t MAX = 8;

  /// Host bits in addr are cleared. Returns false when the list is already full.
  bool add(uint32_t addr, uint32_t mask) {
    if (this->count_ >= MAX) {
      return false;
    }
    this->addr_[this->count_] = addr & mask;
    this->mask_[this->count_] = mask;
    this->count_++;
    return true;
  }

  /// True when the list is empty or addr falls into one entry.
  bool allows(uint32_t addr) const {
    if (this->count_ == 0) {
      return true;
    }
    for (uint8_t i = 0; i < this->count_; i++) {
      if ((addr & this->mask_[i]) == this->addr_[i]) {
        return true;
      }
    }
    return false;
  }

  bool empty() const { return this->count_ == 0; }
  uint8_t size() const { return this->count_; }
  uint32_t addr_at(uint8_t index) const { return this->addr_[index]; }
  uint32_t mask_at(uint8_t index) const { return this->mask_[index]; }

 private:
  uint32_t addr_[MAX]{};
  uint32_t mask_[MAX]{};
  uint8_t count_{0};
};

}  // namespace esphome::socket
