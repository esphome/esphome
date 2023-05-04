#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include <array>
#include <lwip/ip_addr.h>

#ifdef USE_RP2040
#include <Arduino.h>
#endif /* USE_RP2040 */

#ifdef USE_ESP_IDF
#include <cstring>
#include <esp_netif.h>
#endif

namespace esphome {
namespace network {

struct IPAddress {
 public:
  IPAddress() { ip_addr_set_zero(&ip_addr_); }
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    IP_ADDR4(&ip_addr_, first, second, third, fourth);
  }
  IPAddress(uint32_t raw) { ip_addr_set_ip4_u32(&ip_addr_, raw); }
  IPAddress(const ip_addr_t *other_ip) { ip_addr_copy(ip_addr_, *other_ip); }
  bool is_set() { return !ip_addr_isany(&ip_addr_); }
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
  operator ip_addr_t() const { return ip_addr_; };
#if LWIP_IPV6
  operator ip4_addr_t() const { return *ip_2_ip4(&ip_addr_); };
#endif /* LWIP_IPV6 */
#ifdef USE_RP2040
  operator arduino::IPAddress() const { return arduino::IPAddress(&ip_addr_); };
#endif /* USE_RP2040 */
#ifdef USE_ESP_IDF
  operator esp_ip_addr_t() const {
    esp_ip_addr_t tmp;
#if LWIP_IPV6
    memcpy((void *) &tmp, (void *) &ip_addr_, sizeof(ip_addr_));
#else
    memcpy((void *) &tmp.u_addr.ip4, (void *) &ip_addr_, sizeof(ip_addr_));
#endif /* LWIP_IPV6 */
    return tmp;
  }
#endif

 protected:
  ip_addr_t ip_addr_;
};

}  // namespace network
}  // namespace esphome
