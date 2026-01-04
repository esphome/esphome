#include "socket.h"
#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)
#include <cerrno>
#include <cstring>
#include <string>
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::socket {

Socket::~Socket() {}

// Format sockaddr into caller-provided buffer, returns length written (excluding null)
static size_t format_sockaddr_to(const struct sockaddr_storage &storage, std::span<char, SOCKADDR_STR_LEN> buf) {
  if (storage.ss_family == AF_INET) {
    const auto *addr = reinterpret_cast<const struct sockaddr_in *>(&storage);
#ifdef USE_SOCKET_IMPL_LWIP_TCP
    // LWIP raw TCP (ESP8266) uses inet_ntoa_r
    inet_ntoa_r(addr->sin_addr, buf.data(), buf.size());
    return strlen(buf.data());
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
    // LWIP sockets (LibreTiny, ESP32 Arduino) uses lwip_inet_ntop
    if (lwip_inet_ntop(AF_INET, &addr->sin_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
#else
    // BSD sockets (host, ESP32-IDF)
    if (inet_ntop(AF_INET, &addr->sin_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
#endif
  }
#if USE_NETWORK_IPV6
  else if (storage.ss_family == AF_INET6) {
    const auto *addr = reinterpret_cast<const struct sockaddr_in6 *>(&storage);
#ifdef USE_SOCKET_IMPL_LWIP_TCP
    // LWIP raw TCP (ESP8266) uses inet6_ntoa_r
    inet6_ntoa_r(addr->sin6_addr, buf.data(), buf.size());
    return strlen(buf.data());
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
    // LWIP sockets - format IPv4-mapped IPv6 as IPv4
    if (addr->sin6_addr.un.u32_addr[0] == 0 && addr->sin6_addr.un.u32_addr[1] == 0 &&
        addr->sin6_addr.un.u32_addr[2] == htonl(0xFFFF) &&
        lwip_inet_ntop(AF_INET, &addr->sin6_addr.un.u32_addr[3], buf.data(), buf.size()) != nullptr) {
      return strlen(buf.data());
    }
    if (lwip_inet_ntop(AF_INET6, &addr->sin6_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
#else
    // BSD sockets - format IPv4-mapped IPv6 as IPv4
    if (addr->sin6_addr.un.u32_addr[0] == 0 && addr->sin6_addr.un.u32_addr[1] == 0 &&
        addr->sin6_addr.un.u32_addr[2] == htonl(0xFFFF) &&
        inet_ntop(AF_INET, &addr->sin6_addr.un.u32_addr[3], buf.data(), buf.size()) != nullptr) {
      return strlen(buf.data());
    }
    if (inet_ntop(AF_INET6, &addr->sin6_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
#endif
  }
#endif
  buf[0] = '\0';
  return 0;
}

size_t Socket::getpeername_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getpeername(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(storage, buf);
}

size_t Socket::getsockname_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getsockname(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(storage, buf);
}

std::unique_ptr<Socket> socket_ip(int type, int protocol) {
#if USE_NETWORK_IPV6
  return socket(AF_INET6, type, protocol);
#else
  return socket(AF_INET, type, protocol);
#endif /* USE_NETWORK_IPV6 */
}

std::unique_ptr<Socket> socket_ip_loop_monitored(int type, int protocol) {
#if USE_NETWORK_IPV6
  return socket_loop_monitored(AF_INET6, type, protocol);
#else
  return socket_loop_monitored(AF_INET, type, protocol);
#endif /* USE_NETWORK_IPV6 */
}

socklen_t set_sockaddr(struct sockaddr *addr, socklen_t addrlen, const std::string &ip_address, uint16_t port) {
#if USE_NETWORK_IPV6
  if (ip_address.find(':') != std::string::npos) {
    if (addrlen < sizeof(sockaddr_in6)) {
      errno = EINVAL;
      return 0;
    }
    auto *server = reinterpret_cast<sockaddr_in6 *>(addr);
    memset(server, 0, sizeof(sockaddr_in6));
    server->sin6_family = AF_INET6;
    server->sin6_port = htons(port);

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
    // Use standard inet_pton for BSD sockets
    if (inet_pton(AF_INET6, ip_address.c_str(), &server->sin6_addr) != 1) {
      errno = EINVAL;
      return 0;
    }
#else
    // Use LWIP-specific functions
    ip6_addr_t ip6;
    inet6_aton(ip_address.c_str(), &ip6);
    memcpy(server->sin6_addr.un.u32_addr, ip6.addr, sizeof(ip6.addr));
#endif
    return sizeof(sockaddr_in6);
  }
#endif /* USE_NETWORK_IPV6 */
  if (addrlen < sizeof(sockaddr_in)) {
    errno = EINVAL;
    return 0;
  }
  auto *server = reinterpret_cast<sockaddr_in *>(addr);
  memset(server, 0, sizeof(sockaddr_in));
  server->sin_family = AF_INET;
  server->sin_addr.s_addr = inet_addr(ip_address.c_str());
  server->sin_port = htons(port);
  return sizeof(sockaddr_in);
}

socklen_t set_sockaddr_any(struct sockaddr *addr, socklen_t addrlen, uint16_t port) {
#if USE_NETWORK_IPV6
  if (addrlen < sizeof(sockaddr_in6)) {
    errno = EINVAL;
    return 0;
  }
  auto *server = reinterpret_cast<sockaddr_in6 *>(addr);
  memset(server, 0, sizeof(sockaddr_in6));
  server->sin6_family = AF_INET6;
  server->sin6_port = htons(port);
  server->sin6_addr = IN6ADDR_ANY_INIT;
  return sizeof(sockaddr_in6);
#else
  if (addrlen < sizeof(sockaddr_in)) {
    errno = EINVAL;
    return 0;
  }
  auto *server = reinterpret_cast<sockaddr_in *>(addr);
  memset(server, 0, sizeof(sockaddr_in));
  server->sin_family = AF_INET;
  server->sin_addr.s_addr = ESPHOME_INADDR_ANY;
  server->sin_port = htons(port);
  return sizeof(sockaddr_in);
#endif /* USE_NETWORK_IPV6 */
}
}  // namespace esphome::socket
#endif
