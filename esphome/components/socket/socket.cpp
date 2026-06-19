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
#if defined(USE_HOST) || defined(USE_ZEPHYR)
#include <ifaddrs.h>
#include <net/if.h>
#else
#include "lwip/netif.h"
#endif

namespace esphome::socket {

#ifdef USE_HOST
// Shared ready() implementation for fd-based socket implementations (BSD and LWIP sockets).
// Checks if the host wake select() loop has marked this fd as ready.
bool socket_ready_fd(int fd, bool loop_monitored) { return !loop_monitored || wake_fd_ready(fd); }
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

std::unique_ptr<ListenSocket> socket_ip_loop_monitored(int type, int protocol) {
#if USE_NETWORK_IPV6
  const int af = AF_INET6;
#else
  const int af = AF_INET;
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  return socket_listen_loop_monitored(af, type, protocol);
#else
  auto sock = socket_loop_monitored(af, type, protocol);
#if USE_NETWORK_IPV6
  if (sock != nullptr) {
    int disable = 0;
    if (sock->setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(disable)) < 0) {
      return nullptr;
    }
  }
#endif
  return sock;
#endif
}

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
socklen_t join_multicast_group(Socket *sock, struct sockaddr *addr, socklen_t addrlen, const char *ip_address,
                               uint16_t port, uint8_t *if_index_out) {
  if (strchr(ip_address, ':') == nullptr) {
    if (addrlen < sizeof(sockaddr_in)) {
      errno = EINVAL;
      return 0;
    }
    struct in_addr ipv4mc {};
#ifdef USE_ZEPHYR
    if (zsock_inet_pton(AF_INET, ip_address, &ipv4mc) != 1) {
#else
    if (inet_aton(ip_address, &ipv4mc) == 0) {
#endif
      errno = EINVAL;
      return 0;
    }
    struct ip_mreq imreq {};
    imreq.imr_interface.s_addr = ESPHOME_INADDR_ANY;
    imreq.imr_multiaddr = ipv4mc;
    if (sock->setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq)) < 0) {
      return 0;
    }
    if (if_index_out != nullptr) {
      *if_index_out = 0;
    }
    return set_sockaddr(addr, addrlen, ip_address, port);
  }
#if USE_NETWORK_IPV6
  if (addrlen < sizeof(sockaddr_in6)) {
    errno = EINVAL;
    return 0;
  }
  struct ipv6_mreq imreq6 {};
#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
#ifdef USE_ZEPHYR
  if (zsock_inet_pton(AF_INET6, ip_address, &imreq6.ipv6mr_multiaddr) != 1) {
#else
  if (inet_pton(AF_INET6, ip_address, &imreq6.ipv6mr_multiaddr) != 1) {
#endif
    errno = EINVAL;
    return 0;
  }
#else
  ip6_addr_t ip6;
  if (!inet6_aton(ip_address, &ip6)) {
    errno = EINVAL;
    return 0;
  }
  memcpy(imreq6.ipv6mr_multiaddr.un.u32_addr, ip6.addr, sizeof(ip6.addr));
#endif
  // Both POSIX and LwIP require an explicit interface index.
  // POSIX: interface=0 fails for link-local multicast (ff02::) and may select loopback for
  // other scopes. LwIP: NETIF_NO_INDEX=0 is always rejected. Iterate directly to a real
  // non-loopback interface on both platforms.
  imreq6.ipv6mr_interface = 0;
  bool joined = false;
#if defined(USE_HOST) || defined(USE_ZEPHYR)
  struct ifaddrs *ifaddr;
  if (getifaddrs(&ifaddr) == 0) {
    for (struct ifaddrs *ifa = ifaddr; ifa != nullptr && !joined; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET6) {
        continue;
      }
      if ((ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_UP)) {
        continue;
      }
      unsigned int idx = if_nametoindex(ifa->ifa_name);
      if (idx == 0) {
        continue;
      }
      imreq6.ipv6mr_interface = idx;
      if (sock->setsockopt(IPPROTO_IPV6, IPV6_JOIN_GROUP, &imreq6, sizeof(imreq6)) == 0) {
        joined = true;
      }
    }
    freeifaddrs(ifaddr);
  }
#else  // embedded — LwIP NETIF_FOREACH available on all embedded targets
  struct netif *netif;
  NETIF_FOREACH(netif) {
    if (netif->name[0] == 'l' && netif->name[1] == 'o') {
      continue;
    }
    imreq6.ipv6mr_interface = netif_get_index(netif);
    if (sock->setsockopt(IPPROTO_IPV6, IPV6_JOIN_GROUP, &imreq6, sizeof(imreq6)) == 0) {
      joined = true;
      break;
    }
  }
#endif
  if (!joined) {
    return 0;
  }
  if (if_index_out != nullptr) {
    *if_index_out = static_cast<uint8_t>(imreq6.ipv6mr_interface);
  }
  socklen_t filled = set_sockaddr(addr, addrlen, ip_address, port);
#if defined(USE_HOST) || defined(USE_ZEPHYR)
  if (filled == sizeof(sockaddr_in6)) {
    // POSIX bind() on link-local multicast (ff02::, ff12::) requires sin6_scope_id
    // set to the interface index. Harmless for site-local and global scopes.
    // Not needed on LwIP which ignores sin6_scope_id in bind().
    reinterpret_cast<sockaddr_in6 *>(addr)->sin6_scope_id = imreq6.ipv6mr_interface;
  }
#endif
  return filled;
#else
  errno = EINVAL;
  return 0;
#endif
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
    // Use standard inet_pton for BSD sockets
    if (inet_pton(AF_INET6, ip_address, &server->sin6_addr) != 1) {
      errno = EINVAL;
      return 0;
    }
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
  server->sin_addr.s_addr = inet_addr(ip_address);
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
