#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include <array>
#include <lwip/ip_addr.h>

#ifdef USE_RP2040
#include <Arduino.h>
#endif /* USE_RP2040 */
#if USE_ARDUINO
#include <Arduino.h>
#endif /* USE_ADRDUINO */

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
#ifdef USE_ESP32
#if LWIP_IPV6
  IPAddress(esp_ip4_addr_t *other_ip) {
    memcpy((void *) &ip_addr_.u_addr.ip4, (void *) other_ip, sizeof(esp_ip4_addr_t));
  }
#else
  IPAddress(esp_ip4_addr_t *other_ip) { memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(ip_addr_)); }
#endif /* LWIP_IPV6 */
#endif /* USE_ESP32 */
#if USE_ESP32_FRAMEWORK_ARDUINO
  IPAddress(const Arduino_h::IPAddress &other_ip) { ip_addr_set_ip4_u32(&ip_addr_, other_ip); }
#endif /* USE_ESP32_FRAMEWORK_ARDUINO */
  bool is_set() { return !ip_addr_isany(&ip_addr_); }
  std::string str() const { return ipaddr_ntoa(&ip_addr_); }
  bool operator==(const IPAddress &other) const { return ip_addr_cmp(&ip_addr_, &other.ip_addr_); }
  bool operator!=(const IPAddress &other) const { return !(&ip_addr_ == &other.ip_addr_); }
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

  operator ip_addr_t() const { return ip_addr_; };
#if LWIP_IPV6
  operator ip4_addr_t() const { return *ip_2_ip4(&ip_addr_); };
#endif /* LWIP_IPV6 */

#ifdef USE_RP2040
  operator arduino::IPAddress() const { return arduino::IPAddress(&ip_addr_); };
#endif /* USE_RP2040 */

#if USE_ESP32_FRAMEWORK_ARDUINO
  operator Arduino_h::IPAddress() const { return ip_addr_get_ip4_u32(&ip_addr_); };
#endif /* USE_ESP32_FRAMEWORK_ADRDUINO */

#ifdef USE_ESP32
  operator esp_ip_addr_t() const {
    esp_ip_addr_t tmp;
#if LWIP_IPV6
    memcpy((void *) &tmp, (void *) &ip_addr_, sizeof(ip_addr_));
#else
    memcpy((void *) &tmp.u_addr.ip4, (void *) &ip_addr_, sizeof(ip_addr_));
#endif /* LWIP_IPV6 */
    return tmp;
  }
#endif /* USE_ESP32 */

#if USE_ESP32
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

 protected:
  ip_addr_t ip_addr_;
};

}  // namespace network
}  // namespace esphome
