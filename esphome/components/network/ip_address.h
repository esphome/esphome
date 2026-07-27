#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include <cstdint>
#include <string>
#include <cstdio>
#include <array>
#include <cstring>
#include "esphome/core/helpers.h"
#include "esphome/core/macros.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY) || USE_ARDUINO_VERSION_CODE > VERSION_CODE(3, 0, 0)
#include <lwip/ip_addr.h>
#endif
#if USE_ARDUINO
#include <Arduino.h>
#include <IPAddress.h>
#endif /* USE_ARDUINO */

#if defined(USE_HOST) || defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
#include <arpa/inet.h>
#if USE_NETWORK_IPV6
using ip4_addr_t = struct in_addr;
using ip6_addr_t = struct in6_addr;
struct ip_addr_t {
  union {
    struct in6_addr ip6;
    struct in_addr ip4;
  } u_addr;
  uint8_t type;
};
enum : uint8_t { IPADDR_TYPE_V4 = 0, IPADDR_TYPE_V6 = 6 };
static inline int ipaddr_aton(const char *cp, ip_addr_t *addr) {
  if (strchr(cp, ':') != nullptr) {
    if (inet_pton(AF_INET6, cp, &addr->u_addr.ip6) != 1) {
      return 0;
    }
    addr->type = IPADDR_TYPE_V6;
    return 1;
  }
  if (inet_aton(cp, &addr->u_addr.ip4) != 1) {
    return 0;
  }
  addr->type = IPADDR_TYPE_V4;
  return 1;
}
#else
using ip_addr_t = in_addr;
using ip4_addr_t = in_addr;
#define ipaddr_aton(x, y) inet_aton((x), (y))
#endif  // USE_NETWORK_IPV6
#endif  // USE_HOST || USE_ZEPHYR_VARIANT_NATIVE_SIM

#if defined(USE_ZEPHYR) && !defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
#if defined(CONFIG_NET_IPV6)
using ip_addr_t = struct in6_addr;
static inline int ipaddr_aton(const char *cp, ip_addr_t *addr) { return inet_pton(AF_INET6, cp, addr) == 1 ? 1 : 0; }
#else
using ip_addr_t = struct in_addr;
using ip4_addr_t = struct in_addr;
static inline int ipaddr_aton(const char *cp, ip_addr_t *addr) { return inet_pton(AF_INET, cp, addr) == 1 ? 1 : 0; }
#endif /* CONFIG_NET_IPV6 */
#endif /* USE_ZEPHYR */

#if USE_ESP32_FRAMEWORK_ARDUINO
#define arduino_ns Arduino_h
#elif USE_LIBRETINY
#define arduino_ns arduino
#elif USE_ARDUINO
#define arduino_ns
#endif /* USE_ESP32_FRAMEWORK_ARDUINO */

#ifdef USE_ESP32
#include <esp_netif.h>
#endif /* USE_ESP32 */

namespace esphome::network {

/// Buffer size for IP address string (IPv6 max: 39 chars + null)
static constexpr size_t IP_ADDRESS_BUFFER_SIZE =
#if defined(USE_ZEPHYR)
    // Mainline Zephyr's inet_ntop() rejects buffers smaller than NET_INET6_ADDRSTRLEN (46).
    46;
#else
    40;
#endif

/// Lowercase hex digits in IP address string (A-F -> a-f for IPv6 per RFC 5952)
inline void lowercase_ip_str(char *buf) {
  for (char *p = buf; *p; ++p) {
    if (*p >= 'A' && *p <= 'F')
      *p += 32;
  }
}

struct IPAddress {
 public:
#if defined(USE_ZEPHYR) && !defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
#if defined(CONFIG_NET_IPV6)
  IPAddress() { memset(&ip_addr_, 0, sizeof(ip_addr_)); }
#if defined(CONFIG_NET_IPV4)
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    struct in_addr addr4 {
      .s_addr = htonl((first << 24) | (second << 16) | (third << 8) | fourth)
    };
    net_ipv6_addr_create_v4_mapped(&addr4, &ip_addr_);
  }
#endif /* CONFIG_NET_IPV4 */
  IPAddress(const std::string &in_address) : ip_addr_{} { ipaddr_aton(in_address.c_str(), &ip_addr_); }
  IPAddress(const struct in6_addr *other_ip) { ip_addr_ = *other_ip; }
  IPAddress(const struct sockaddr_in6 *addr) { ip_addr_ = addr->sin6_addr; }

  operator struct in6_addr() const { return ip_addr_; }

  bool is_set() const { return !net_ipv6_is_addr_unspecified(&ip_addr_); }
#if defined(CONFIG_NET_IPV4)
  // V4-mapped (::ffff:a.b.c.d) addresses represent IPv4 traffic carried over the AF_INET6 socket
  // Zephyr always uses when CONFIG_NET_IPV6 is on; anything else is real IPv6.
  bool is_ip4() const { return this->is_set() && net_ipv6_addr_is_v4_mapped(&ip_addr_); }
  bool is_ip6() const { return this->is_set() && !net_ipv6_addr_is_v4_mapped(&ip_addr_); }
#else
  bool is_ip4() const { return false; }
  bool is_ip6() const { return this->is_set(); }
#endif /* CONFIG_NET_IPV4 */
  bool is_multicast() const { return net_ipv6_is_addr_mcast(&ip_addr_); }
  char *str_to(char *buf) const {
#if defined(CONFIG_NET_IPV4)
    if (this->is_ip4()) {
      struct in_addr addr4 {};
      addr4.s_addr = ip_addr_.s6_addr32[3];
      if (inet_ntop(AF_INET, &addr4, buf, IP_ADDRESS_BUFFER_SIZE) == nullptr)
        buf[0] = '\0';
      return buf;
    }
#endif /* CONFIG_NET_IPV4 */
    if (inet_ntop(AF_INET6, &ip_addr_, buf, IP_ADDRESS_BUFFER_SIZE) == nullptr)
      buf[0] = '\0';
    return buf;
  }
  bool operator==(const IPAddress &other) const { return net_ipv6_addr_cmp(&ip_addr_, &other.ip_addr_); }
  bool operator!=(const IPAddress &other) const { return !net_ipv6_addr_cmp(&ip_addr_, &other.ip_addr_); }

#else  // only CONFIG_NET_IPV4 branch
  // enable_ipv6: false -- plain in_addr, not the IPv6-only in6_addr container the branch above
  // uses for the dual-stack (V4-mapped) and IPv6-only cases.
  IPAddress() { ip_addr_.s_addr = 0; }
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    this->ip_addr_.s_addr = htonl((first << 24) | (second << 16) | (third << 8) | fourth);
  }
  IPAddress(const std::string &in_address) { ipaddr_aton(in_address.c_str(), &ip_addr_); }
  IPAddress(const ip_addr_t *other_ip) { ip_addr_ = *other_ip; }
  char *str_to(char *buf) const {
    if (inet_ntop(AF_INET, &ip_addr_, buf, IP_ADDRESS_BUFFER_SIZE) == nullptr)
      buf[0] = '\0';
    return buf;
  }
  bool is_set() const { return ip_addr_.s_addr != 0; }
  bool is_ip4() const { return true; }
  bool is_ip6() const { return false; }
  // 224.0.0.0/4 (RFC 5771)
  bool is_multicast() const { return (ntohl(ip_addr_.s_addr) & 0xF0000000) == 0xE0000000; }
  bool operator==(const IPAddress &other) const { return ip_addr_.s_addr == other.ip_addr_.s_addr; }
  bool operator!=(const IPAddress &other) const { return ip_addr_.s_addr != other.ip_addr_.s_addr; }
#endif /* CONFIG_NET_IPV6 */

#elif defined(USE_HOST) || defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
#if USE_NETWORK_IPV6
  IPAddress() { memset(&this->ip_addr_, 0, sizeof(this->ip_addr_)); }
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    memset(&this->ip_addr_, 0, sizeof(this->ip_addr_));
    this->ip_addr_.u_addr.ip4.s_addr =
        htonl(((uint32_t) first << 24) | ((uint32_t) second << 16) | ((uint32_t) third << 8) | fourth);
    this->ip_addr_.type = IPADDR_TYPE_V4;
  }
  IPAddress(const char *in_address) {
    memset(&this->ip_addr_, 0, sizeof(this->ip_addr_));
    ipaddr_aton(in_address, &this->ip_addr_);
  }
  IPAddress(const std::string &in_address) : IPAddress(in_address.c_str()) {}
  IPAddress(const ip_addr_t *other_ip) { memcpy(&this->ip_addr_, other_ip, sizeof(ip_addr_t)); }
  IPAddress(ip4_addr_t *other_ip) {
    memset(&this->ip_addr_, 0, sizeof(this->ip_addr_));
    this->ip_addr_.u_addr.ip4 = *other_ip;
    this->ip_addr_.type = IPADDR_TYPE_V4;
  }
  IPAddress(ip6_addr_t *other_ip) {
    memset(&this->ip_addr_, 0, sizeof(this->ip_addr_));
    this->ip_addr_.u_addr.ip6 = *other_ip;
    this->ip_addr_.type = IPADDR_TYPE_V6;
  }
  operator ip_addr_t() const { return this->ip_addr_; }
  bool is_set() const {
    if (this->ip_addr_.type == IPADDR_TYPE_V6) {
      static constexpr uint8_t zero[sizeof(struct in6_addr)] = {};
      return memcmp(this->ip_addr_.u_addr.ip6.s6_addr, zero, sizeof(zero)) != 0;
    }
    return this->ip_addr_.u_addr.ip4.s_addr != 0;
  }
  bool is_ip4() const { return this->ip_addr_.type == IPADDR_TYPE_V4; }
  bool is_ip6() const { return this->ip_addr_.type == IPADDR_TYPE_V6; }
  bool is_multicast() const {
    if (this->ip_addr_.type == IPADDR_TYPE_V6) {
      return this->ip_addr_.u_addr.ip6.s6_addr[0] == 0xff;
    }
    return (ntohl(this->ip_addr_.u_addr.ip4.s_addr) & 0xF0000000UL) == 0xE0000000UL;
  }
  // Remove before 2026.8.0
  ESPDEPRECATED(
      "str() is deprecated: use 'char buf[IP_ADDRESS_BUFFER_SIZE]; ip.str_to(buf);' instead. Removed in 2026.8.0",
      "2026.2.0")
  std::string str() const {
    char buf[IP_ADDRESS_BUFFER_SIZE];
    this->str_to(buf);
    return buf;
  }
  char *str_to(char *buf) const {
    if (this->ip_addr_.type == IPADDR_TYPE_V6) {
      inet_ntop(AF_INET6, &this->ip_addr_.u_addr.ip6, buf, IP_ADDRESS_BUFFER_SIZE);
    } else {
      inet_ntop(AF_INET, &this->ip_addr_.u_addr.ip4, buf, IP_ADDRESS_BUFFER_SIZE);
    }
    lowercase_ip_str(buf);
    return buf;
  }
  bool operator==(const IPAddress &other) const {
    if (this->ip_addr_.type != other.ip_addr_.type) {
      return false;
    }
    if (this->ip_addr_.type == IPADDR_TYPE_V6) {
      return memcmp(&this->ip_addr_.u_addr.ip6, &other.ip_addr_.u_addr.ip6, sizeof(struct in6_addr)) == 0;
    }
    return this->ip_addr_.u_addr.ip4.s_addr == other.ip_addr_.u_addr.ip4.s_addr;
  }
  bool operator!=(const IPAddress &other) const { return !(*this == other); }
  IPAddress &operator+=(uint8_t increase) {
    if (this->ip_addr_.type == IPADDR_TYPE_V4) {
      (((uint8_t *) (&this->ip_addr_.u_addr.ip4))[3]) += increase;
    }
    return *this;
  }
#else
  IPAddress() { ip_addr_.s_addr = 0; }
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    this->ip_addr_.s_addr = htonl((first << 24) | (second << 16) | (third << 8) | fourth);
  }
  IPAddress(const std::string &in_address) { inet_aton(in_address.c_str(), &ip_addr_); }
  IPAddress(const ip_addr_t *other_ip) { ip_addr_ = *other_ip; }
  /// Write IP address to buffer. Buffer must be at least IP_ADDRESS_BUFFER_SIZE bytes.
  char *str_to(char *buf) const {
    inet_ntop(AF_INET, &ip_addr_, buf, IP_ADDRESS_BUFFER_SIZE);
    return buf;  // IPv4 only, no hex letters to lowercase
  }
#endif  // USE_NETWORK_IPV6
#else   // LWIP
  IPAddress() { ip_addr_set_zero(&ip_addr_); }
#if LWIP_IPV4
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    IP_ADDR4(&ip_addr_, first, second, third, fourth);
  }
#endif /* LWIP_IPV4 */
  IPAddress(const ip_addr_t *other_ip) { ip_addr_copy(ip_addr_, *other_ip); }
  IPAddress(const char *in_address) { ipaddr_aton(in_address, &ip_addr_); }
  IPAddress(const std::string &in_address) { ipaddr_aton(in_address.c_str(), &ip_addr_); }
#if LWIP_IPV4
  IPAddress(ip4_addr_t *other_ip) {
    memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(ip4_addr_t));
#if LWIP_IPV6
    ip_addr_.type = IPADDR_TYPE_V4;
#endif /* LWIP_IPV6 */
  }
#endif /* LWIP_IPV4 */
#if USE_ARDUINO
  IPAddress(const arduino_ns::IPAddress &other_ip) { ip_addr_set_ip4_u32(&ip_addr_, other_ip); }
#endif /* USE_ARDUINO */
#if LWIP_IPV6
  IPAddress(ip6_addr_t *other_ip) {
    memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(ip6_addr_t));
#if LWIP_IPV4
    ip_addr_.type = IPADDR_TYPE_V6;
#endif /* LWIP_IPV4 */
  }
#endif /* LWIP_IPV6 */

#ifdef USE_ESP32
#if LWIP_IPV6
  IPAddress(esp_ip6_addr_t *other_ip) {
#if LWIP_IPV4
    memcpy((void *) &ip_addr_.u_addr.ip6, (void *) other_ip, sizeof(esp_ip6_addr_t));
    ip_addr_.type = IPADDR_TYPE_V6;
#else
    memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(esp_ip6_addr_t));
#endif /* LWIP_IPV4 */
  }
#endif /* LWIP_IPV6 */
#if LWIP_IPV4
  IPAddress(esp_ip4_addr_t *other_ip) {
    memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(esp_ip4_addr_t));
#if LWIP_IPV6
    ip_addr_.type = IPADDR_TYPE_V4;
#endif /* LWIP_IPV6 */
  }
#endif /* LWIP_IPV4 */
  IPAddress(esp_ip_addr_t *other_ip) {
#if LWIP_IPV6
    memcpy((void *) &ip_addr_, (void *) other_ip, sizeof(ip_addr_));
#elif LWIP_IPV4
    memcpy((void *) &ip_addr_, (void *) &other_ip->u_addr.ip4, sizeof(ip_addr_));
#endif /* LWIP_IPV6 */
  }
  operator esp_ip_addr_t() const {
    esp_ip_addr_t tmp;
#if LWIP_IPV6
    memcpy((void *) &tmp, (void *) &ip_addr_, sizeof(ip_addr_));
#elif LWIP_IPV4
    memcpy((void *) &tmp.u_addr.ip4, (void *) &ip_addr_, sizeof(ip_addr_));
#endif /* LWIP_IPV6 */
    return tmp;
  }
#if LWIP_IPV4
  operator esp_ip4_addr_t() const {
    esp_ip4_addr_t tmp;
#if LWIP_IPV6
    memcpy((void *) &tmp, (void *) &ip_addr_.u_addr.ip4, sizeof(esp_ip4_addr_t));
#else
    memcpy((void *) &tmp, (void *) &ip_addr_, sizeof(ip_addr_));
#endif /* LWIP_IPV6 */
    return tmp;
  }
#endif /* LWIP_IPV4 */
#endif /* USE_ESP32 */

  operator ip_addr_t() const { return ip_addr_; }
#if LWIP_IPV6 && LWIP_IPV4
  operator ip4_addr_t() const { return *ip_2_ip4(&ip_addr_); }
#endif /* LWIP_IPV6 && LWIP_IPV4 */

#if USE_ARDUINO
  operator arduino_ns::IPAddress() const { return ip_addr_get_ip4_u32(&ip_addr_); }
#endif /* USE_ARDUINO */

  bool is_set() const { return !ip_addr_isany(&ip_addr_); }  // NOLINT(readability-simplify-boolean-expr)
  bool is_ip4() const { return IP_IS_V4(&ip_addr_); }
  bool is_ip6() const { return IP_IS_V6(&ip_addr_); }
  bool is_multicast() const { return ip_addr_ismulticast(&ip_addr_); }
  /// Write IP address to buffer. Buffer must be at least IP_ADDRESS_BUFFER_SIZE bytes.
  /// Output is lowercased per RFC 5952 (IPv6 hex digits a-f).
  char *str_to(char *buf) const {
    ipaddr_ntoa_r(&ip_addr_, buf, IP_ADDRESS_BUFFER_SIZE);
    lowercase_ip_str(buf);
    return buf;
  }
  bool operator==(const IPAddress &other) const { return ip_addr_cmp(&ip_addr_, &other.ip_addr_); }
  bool operator!=(const IPAddress &other) const { return !ip_addr_cmp(&ip_addr_, &other.ip_addr_); }
  IPAddress &operator+=(uint8_t increase) {
    if (IP_IS_V4(&ip_addr_)) {
#if LWIP_IPV6 && LWIP_IPV4
      (((u8_t *) (&ip_addr_.u_addr.ip4))[3]) += increase;
#elif LWIP_IPV4
      (((u8_t *) (&ip_addr_.addr))[3]) += increase;
#endif  /* LWIP_IPV6 && LWIP_IPV4 */
    }
    return *this;
  }
#endif  // LWIP

 protected:
  ip_addr_t ip_addr_;
};

using IPAddresses = std::array<IPAddress, 5>;

}  // namespace esphome::network
#endif /* NETWORK */
