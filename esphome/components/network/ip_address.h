#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include <array>
#include <lwip/ip_addr.h>

#if USE_ARDUINO
#include <Arduino.h>
#include <IPAddress.h>
#endif /* USE_ADRDUINO */

#if USE_ESP32_FRAMEWORK_ARDUINO
#define arduino_ns Arduino_h
#elif USE_LIBRETINY
#define arduino_ns arduino
#elif USE_ARDUINO
#define arduino_ns
#endif

#ifdef USE_ESP32
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
  IPAddress(const ip_addr_t *other_ip) { ip_addr_copy(ip_addr_, *other_ip); }
  IPAddress(const std::string &in_address) { ipaddr_aton(in_address.c_str(), &ip_addr_); }
  IPAddress(ip4_addr_t *other_ip) { memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(ip4_addr_t)); }
#if USE_ARDUINO
  IPAddress(const arduino_ns::IPAddress &other_ip) { ip_addr_set_ip4_u32(&ip_addr_, other_ip); }
#endif
#if LWIP_IPV6
  IPAddress(ip6_addr_t *other_ip) {
    memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(ip6_addr_t));
    ip_addr_.type = IPADDR_TYPE_V6;
  }
#endif /* LWIP_IPV6 */

#ifdef USE_ESP32
#if LWIP_IPV6
  IPAddress(esp_ip6_addr_t *other_ip) {
    memcpy((void *) &ip_addr_.u_addr.ip6, (void *) other_ip, sizeof(esp_ip6_addr_t));
    ip_addr_.type = IPADDR_TYPE_V6;
  }
#endif /* LWIP_IPV6 */
  IPAddress(esp_ip4_addr_t *other_ip) { memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(esp_ip4_addr_t)); }
  operator esp_ip_addr_t() const {
    esp_ip_addr_t tmp;
#if LWIP_IPV6
    memcpy((void *) &tmp, (void *) &ip_addr_, sizeof(ip_addr_));
#else
    memcpy((void *) &tmp.u_addr.ip4, (void *) &ip_addr_, sizeof(ip_addr_));
#endif /* LWIP_IPV6 */
    return tmp;
  }
  operator esp_ip4_addr_t() const {
    esp_ip4_addr_t tmp;
#if LWIP_IPV6
    memcpy((void *) &tmp, (void *) &ip_addr_.u_addr.ip4, sizeof(esp_ip4_addr_t));
#else
    memcpy((void *) &tmp, (void *) &ip_addr_, sizeof(ip_addr_));
#endif /* LWIP_IPV6 */
    return tmp;
  }
#endif /* USE_ESP32 */

  operator ip_addr_t() const { return ip_addr_; }
#if LWIP_IPV6
  operator ip4_addr_t() const { return *ip_2_ip4(&ip_addr_); }
#endif /* LWIP_IPV6 */

#if USE_ARDUINO
  operator arduino_ns::IPAddress() const { return ip_addr_get_ip4_u32(&ip_addr_); }
#endif

  bool is_set() { return !ip_addr_isany(&ip_addr_); }
  bool is_ip4() { return IP_IS_V4(&ip_addr_); }
  bool is_ip6() { return IP_IS_V6(&ip_addr_); }
  std::string str() const { return ipaddr_ntoa(&ip_addr_); }
  bool operator==(const IPAddress &other) const { return ip_addr_cmp(&ip_addr_, &other.ip_addr_); }
  bool operator!=(const IPAddress &other) const { return !ip_addr_cmp(&ip_addr_, &other.ip_addr_); }
  IPAddress &operator+=(uint8_t increase) {
    if (IP_IS_V4(&ip_addr_)) {
#if LWIP_IPV6
      (((u8_t *) (&ip_addr_.u_addr.ip4))[3]) += increase;
#else
      (((u8_t *) (&ip_addr_.addr))[3]) += increase;
#endif /* LWIP_IPV6 */
    }
    return *this;
  }

 protected:
  ip_addr_t ip_addr_;
};

}  // namespace network
}  // namespace esphome
