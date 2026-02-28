#pragma once
#include "esphome/core/helpers.h"

#include <cstdint>
#include <array>

namespace esphome {
namespace bthome {

using EncryptionKey = std::array<uint8_t, 16>;

struct BTHomeHeader {
  uint8_t encrypted : 1;      // bit 0: encrypted data
  uint8_t : 1;                // bit 1: reserved
  uint8_t trigger_based : 1;  // bit 2: irregular advertisement interval
  uint8_t : 2;                // bits 3-4: reserved
  uint8_t version : 3;        // bits 5-7: BTHome version (currently 1 or 2)
};

class MacAddressPtr;
class __attribute__((packed)) MacAddress {
 public:
  MacAddress() = default;
  MacAddress(const uint8_t *addr);
  MacAddress(uint64_t addr);
  MacAddress(const MacAddressPtr &addr);

  MacAddress &operator=(const uint8_t *addr);

  operator const uint8_t *() const;

  bool operator==(const MacAddress &other) const;
  bool operator==(const MacAddressPtr &other) const;

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

  bool operator==(const MacAddressPtr &other) const;
  bool operator==(const MacAddress &other) const;

  const char *c_str() const;

 protected:
  const uint8_t *addr_{nullptr};
};

}  // namespace bthome
}  // namespace esphome
