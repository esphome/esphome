#include "socket.h"
#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)
#include <cerrno>
#include <cstring>
#include <string>
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#ifdef USE_HOST
#include "esphome/core/wake.h"
#endif

namespace esphome::socket {

#ifdef USE_HOST
// Host: ready when the wake select() loop has flagged this fd (or it isn't monitored).
bool socket_ready_fd(int fd, bool loop_monitored) { return !loop_monitored || wake_fd_ready(fd); }
#elif defined(USE_ZEPHYR)
// Zephyr (nRF52): fd monitoring isn't wired into the esphome select loop
// (wake_register_fd is USE_HOST-only), so loop_monitored is always false. Always
// return true — the caller handles EAGAIN/EWOULDBLOCK on read.
//
// Cost (known trade-off, not an oversight): loop-monitored sockets (API, web_server)
// are read every loop() iteration and bail on EAGAIN; there is no event-driven wake,
// so the main loop busy-polls at loop frequency and cannot idle between packets.
// TODO: wire Zephyr fds into an event-driven wake source (e.g. zsock_poll/k_poll) so
// the loop can sleep between packets on battery/OpenThread targets.
bool socket_ready_fd(int /*fd*/, bool /*loop_monitored*/) { return true; }
#endif

// Platform-specific inet_ntop wrappers
#if defined(USE_SOCKET_IMPL_LWIP_TCP)
// LWIP raw TCP (ESP8266) uses inet_ntoa_r which takes struct by value
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  inet_ntoa_r(*reinterpret_cast<const struct in_addr *>(addr), buf, size);
  return buf;
}
#if USE_NETWORK_IPV6
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  inet6_ntoa_r(*reinterpret_cast<const ip6_addr_t *>(addr), buf, size);
  return buf;
}
#endif
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
// LWIP sockets (LibreTiny, ESP32 Arduino)
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  return lwip_inet_ntop(AF_INET, addr, buf, size);
}
#if USE_NETWORK_IPV6
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  return lwip_inet_ntop(AF_INET6, addr, buf, size);
}
#endif
#elif defined(USE_ZEPHYR)
// Zephyr BSD sockets — use Zephyr native address formatting via POSIX-subset wrappers.
// <zephyr/net/socket.h> is already included transitively through <sys/socket.h>.
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  return zsock_inet_ntop(AF_INET, addr, buf, size);
}
// IPv6 is always enabled on nRF52 (config validation enforces enable_ipv6=True),
// but the guard is retained for consistency with other platform blocks.
#if USE_NETWORK_IPV6
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  return zsock_inet_ntop(AF_INET6, addr, buf, size);
}
#endif
#else
// BSD sockets (host, ESP32-IDF)
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  return inet_ntop(AF_INET, addr, buf, size);
}
#if USE_NETWORK_IPV6
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  return inet_ntop(AF_INET6, addr, buf, size);
}
#endif
#endif

// Format sockaddr into caller-provided buffer, returns length written (excluding null)
size_t format_sockaddr_to(const struct sockaddr *addr_ptr, socklen_t len, std::span<char, SOCKADDR_STR_LEN> buf) {
  if (addr_ptr->sa_family == AF_INET && len >= sizeof(const struct sockaddr_in)) {
    const auto *addr = reinterpret_cast<const struct sockaddr_in *>(addr_ptr);
    if (esphome_inet_ntop4(&addr->sin_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
  }
#if USE_NETWORK_IPV6
  else if (addr_ptr->sa_family == AF_INET6 && len >= sizeof(sockaddr_in6)) {
    const auto *addr = reinterpret_cast<const struct sockaddr_in6 *>(addr_ptr);
#ifdef USE_HOST
    // Format IPv4-mapped IPv6 addresses as regular IPv4 (POSIX layout, no LWIP union)
    if (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr) &&
        esphome_inet_ntop4(&addr->sin6_addr.s6_addr[12], buf.data(), buf.size()) != nullptr) {
      return strlen(buf.data());
    }
#elif defined(USE_ZEPHYR)
    // Format IPv4-mapped IPv6 addresses as regular IPv4. Zephyr uses the standard POSIX
    // s6_addr layout (not the LWIP union) but provides no IN6_IS_ADDR_V4MAPPED macro, so
    // detect the ::ffff:0:0/96 prefix directly on the address words.
    if (addr->sin6_addr.s6_addr32[0] == 0 && addr->sin6_addr.s6_addr32[1] == 0 &&
        addr->sin6_addr.s6_addr32[2] == htonl(0xFFFF) &&
        esphome_inet_ntop4(&addr->sin6_addr.s6_addr32[3], buf.data(), buf.size()) != nullptr) {
      return strlen(buf.data());
    }
#elif !defined(USE_SOCKET_IMPL_LWIP_TCP)
    // Format IPv4-mapped IPv6 addresses as regular IPv4 (LWIP layout)
    if (addr->sin6_addr.un.u32_addr[0] == 0 && addr->sin6_addr.un.u32_addr[1] == 0 &&
        addr->sin6_addr.un.u32_addr[2] == htonl(0xFFFF) &&
        esphome_inet_ntop4(&addr->sin6_addr.un.u32_addr[3], buf.data(), buf.size()) != nullptr) {
      return strlen(buf.data());
    }
#endif
    if (esphome_inet_ntop6(&addr->sin6_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
  }
#endif
  buf[0] = '\0';
  return 0;
}

std::unique_ptr<Socket> socket_ip(int type, int protocol) {
#if USE_NETWORK_IPV6
  return socket(AF_INET6, type, protocol);
#else
  return socket(AF_INET, type, protocol);
#endif /* USE_NETWORK_IPV6 */
}

#ifdef USE_SOCKET_IMPL_LWIP_TCP
// LWIP_TCP has separate Socket/ListenSocket types — needs out-of-line factory.
// BSD and LWIP_SOCKETS define this inline in socket.h.
std::unique_ptr<ListenSocket> socket_ip_loop_monitored(int type, int protocol) {
#if USE_NETWORK_IPV6
  return socket_listen_loop_monitored(AF_INET6, type, protocol);
#else
  return socket_listen_loop_monitored(AF_INET, type, protocol);
#endif /* USE_NETWORK_IPV6 */
}
#endif

socklen_t set_sockaddr(struct sockaddr *addr, socklen_t addrlen, const char *ip_address, uint16_t port) {
#if USE_NETWORK_IPV6
  if (strchr(ip_address, ':') != nullptr) {
    if (addrlen < sizeof(sockaddr_in6)) {
      errno = EINVAL;
      return 0;
    }
    auto *server = reinterpret_cast<sockaddr_in6 *>(addr);
    memset(server, 0, sizeof(sockaddr_in6));
    server->sin6_family = AF_INET6;
    server->sin6_port = htons(port);

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
#if defined(USE_ZEPHYR)
    // Zephyr BSD sockets: use native address conversion
    if (zsock_inet_pton(AF_INET6, ip_address, &server->sin6_addr) != 1) {
      errno = EINVAL;
      return 0;
    }
#else
    // Use standard inet_pton for BSD sockets
    if (inet_pton(AF_INET6, ip_address, &server->sin6_addr) != 1) {
      errno = EINVAL;
      return 0;
    }
#endif
#else
    // Use LWIP-specific functions
    ip6_addr_t ip6;
    inet6_aton(ip_address, &ip6);
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
#if defined(USE_ZEPHYR)
  // Zephyr BSD sockets: use native address conversion
  if (zsock_inet_pton(AF_INET, ip_address, &server->sin_addr) != 1) {
    errno = EINVAL;
    return 0;
  }
#else
  server->sin_addr.s_addr = inet_addr(ip_address);
#endif
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
