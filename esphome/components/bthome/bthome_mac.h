#pragma once
#include "esphome/core/helpers.h"

#include <cstdint>

namespace esphome {
namespace bthome {

class MacAddress {
 public:
  MacAddress() = default;
  MacAddress(const uint8_t *addr);
  MacAddress(uint64_t addr);

  MacAddress &operator=(const uint8_t *addr);

  operator const uint8_t *() const;

  bool operator==(const uint8_t *other) const;
  bool operator==(const MacAddress &other) const;
  bool operator!=(const uint8_t *other) const;
  bool operator!=(const MacAddress &other) const;

  const char *c_str() const;

 protected:
  uint8_t addr_[MAC_ADDRESS_SIZE]{};
};

class MacAddressPtr {
 public:
  MacAddressPtr() = default;
  MacAddressPtr(const uint8_t *addr) : addr_(addr) {}
  MacAddressPtr(const MacAddress &addr) : MacAddressPtr((const uint8_t *) (addr)) {}

  operator const uint8_t *() const { return this->addr_; }

  bool operator==(const uint8_t *other) const;
  bool operator==(const MacAddressPtr &other) const;
  bool operator!=(const uint8_t *other) const;
  bool operator!=(const MacAddressPtr &other) const;

  const char *c_str() const;

 protected:
  const uint8_t *addr_{nullptr};
};

}  // namespace bthome
}  // namespace esphome
