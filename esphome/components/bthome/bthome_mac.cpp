#include "bthome_mac.h"

#include <cstring>

namespace esphome {
namespace bthome {

MacAddress::MacAddress(const uint8_t *addr) { *this = addr; }

MacAddress::MacAddress(uint64_t addr) {
  for (int i = sizeof(this->addr_) - 1; i >= 0; i--) {
    this->addr_[i] = addr & 0xFF;
    addr >>= 8;
  }
}

MacAddress::MacAddress(const MacAddressPtr &addr) : MacAddress((const uint8_t *) addr) {}

MacAddress &MacAddress::operator=(const uint8_t *addr) {
  std::memcpy(this->addr_, addr, sizeof(this->addr_));
  return *this;
}

MacAddress::operator const uint8_t *() const { return this->addr_; }

bool MacAddress::operator==(const MacAddress &other) const { return MacAddressPtr(*this) == MacAddressPtr(other); }

bool MacAddress::operator==(const MacAddressPtr &other) const { return MacAddressPtr(*this) == other; }

bool MacAddressPtr::operator==(const MacAddressPtr &other) const {
  return std::memcmp(this->addr_, other.addr_, MAC_ADDRESS_SIZE) == 0;
  ;
}

bool MacAddressPtr::operator==(const MacAddress &other) const { return *this == MacAddressPtr(other); }

const char *MacAddressPtr::c_str() const {
  static char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->addr_, buf);
  return buf;
}

const char *MacAddress::c_str() const { return MacAddress(*this).c_str(); }

}  // namespace bthome
}  // namespace esphome
