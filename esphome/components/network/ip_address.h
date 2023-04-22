#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include <array>
#include <lwip/ip_addr.h>

#ifdef USE_RP2040
#include <Arduino.h>
#endif /* USE_RP2040 */

namespace esphome {
namespace network {

struct IPAddress {
 public:
  IPAddress() { ip_addr_set_zero(&ip_addr_); }
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    IP_ADDR4(&ip_addr_, first, second, third, fourth);
  }
  IPAddress(uint32_t raw) { ip_addr_set_ip4_u32(&ip_addr_, raw); }
  operator uint32_t() const { return ip_addr_get_ip4_u32(&ip_addr_); }
  std::string str() const { return ipaddr_ntoa(&ip_addr_); }
  bool operator==(const IPAddress &other) const { return ip_addr_cmp(&ip_addr_, &other.ip_addr_); }
  IPAddress &operator+=(uint8_t increase) {
    if (IP_IS_V4(&ip_addr_)) {
      uint32_t t_val;
      t_val = ip_addr_get_ip4_u32(&ip_addr_);
      t_val += increase;
      ip_addr_set_ip4_u32(&ip_addr_, t_val);
    }
    return *this;
  }
  operator ip4_addr_t() const { return *ip_2_ip4(&ip_addr_); };
#ifdef USE_RP2040
  operator arduino::IPAddress() const { return arduino::IPAddress(&ip_addr_); };
#endif /* USE_RP2040 */

 protected:
  ip_addr_t ip_addr_;
};

}  // namespace network
}  // namespace esphome
