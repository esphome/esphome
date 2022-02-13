#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include <array>

namespace esphome {
namespace network {

struct IPAddress {
 public:
  IPAddress() : addr_({0, 0, 0, 0}) {}
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) : addr_({first, second, third, fourth}) {}
  IPAddress(uint32_t raw) {
    addr_[0] = (uint8_t)(raw >> 0);
    addr_[1] = (uint8_t)(raw >> 8);
    addr_[2] = (uint8_t)(raw >> 16);
    addr_[3] = (uint8_t)(raw >> 24);
  }
  operator uint32_t() const {
    uint32_t res = 0;
    res |= ((uint32_t) addr_[0]) << 0;
    res |= ((uint32_t) addr_[1]) << 8;
    res |= ((uint32_t) addr_[2]) << 16;
    res |= ((uint32_t) addr_[3]) << 24;
    return res;
  }
  std::string str() const {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", addr_[0], addr_[1], addr_[2], addr_[3]);
    return buffer;
  }
  bool operator==(const IPAddress &other) const {
    return addr_[0] == other.addr_[0] && addr_[1] == other.addr_[1] && addr_[2] == other.addr_[2] &&
           addr_[3] == other.addr_[3];
  }
  uint8_t operator[](int index) const { return addr_[index]; }
  uint8_t &operator[](int index) { return addr_[index]; }

 protected:
  std::array<uint8_t, 4> addr_;
};

}  // namespace network
}  // namespace esphome
