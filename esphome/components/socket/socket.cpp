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
#if defined(USE_HOST)
#include <ifaddrs.h>
#include <net/if.h>
#elif defined(USE_ZEPHYR)
#include <zephyr/net/net_if.h>
#else
#include "lwip/netif.h"
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

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
template<typename F> static bool foreach_eligible_ipv6_if(F &&callback) {
#if defined(USE_HOST)
  struct ifaddrs *ifaddr;
  if (getifaddrs(&ifaddr) != 0) {
    return false;
  }
  bool found_any = false;
  for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET6) {
      continue;
    }
    if ((ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_MULTICAST)) {
      continue;
    }
    unsigned int idx = if_nametoindex(ifa->ifa_name);
    if (idx == 0) {
      continue;
    }
    found_any = true;
    if (callback(idx)) {
      break;
    }
  }
  freeifaddrs(ifaddr);
  if (!found_any) {
    errno = EADDRNOTAVAIL;
  }
  return found_any;
#elif defined(USE_ZEPHYR)
  // Zephyr has no getifaddrs()/ifaddrs.h; enumerate interfaces via the native net_if API.
  // NET_IF_IPV6_NO_MLD is the direct analog of the LwIP branch's NETIF_FLAG_MLD6 check below.
  struct EligibleIfContext {
    F *callback;
    bool found_any = false;
  };
  EligibleIfContext ctx{&callback};
  net_if_foreach(
      [](struct net_if *iface, void *user_data) {
        auto *ctx = static_cast<EligibleIfContext *>(user_data);
        if (!net_if_flag_is_set(iface, NET_IF_UP) || !net_if_flag_is_set(iface, NET_IF_IPV6) ||
            net_if_flag_is_set(iface, NET_IF_IPV6_NO_MLD)) {
          return;
        }
        int idx = net_if_get_by_iface(iface);
        if (idx <= 0) {
          return;
        }
        ctx->found_any = true;
        (*ctx->callback)(static_cast<unsigned int>(idx));
      },
      &ctx);
  if (!ctx.found_any) {
    errno = EADDRNOTAVAIL;
  }
  return ctx.found_any;
#else
  bool found_any = false;
  struct netif *netif;
  NETIF_FOREACH(netif) {
    if (netif->name[0] == 'l' && netif->name[1] == 'o') {
      continue;
    }
    if (!(netif->flags & NETIF_FLAG_UP) || !(netif->flags & NETIF_FLAG_MLD6)) {
      continue;
    }
    found_any = true;
    if (callback(netif_get_index(netif))) {
      break;
    }
  }
  if (!found_any) {
    errno = EADDRNOTAVAIL;
  }
  return found_any;
#endif
}

bool join_multicast_group(Socket *sock, const char *ip_address, uint32_t *if_index_out) {
  if (strchr(ip_address, ':') == nullptr) {
    struct in_addr ipv4mc {};
#ifdef USE_ZEPHYR
    if (zsock_inet_pton(AF_INET, ip_address, &ipv4mc) != 1) {
#else
    if (inet_aton(ip_address, &ipv4mc) == 0) {
#endif
      errno = EINVAL;
      return false;
    }
#ifdef USE_ZEPHYR
    // Zephyr's setsockopt(IP_ADD_MEMBERSHIP) requires exactly sizeof(struct ip_mreqn); it has
    // no plain "struct ip_mreq" at all (unlike POSIX/lwIP).
    struct ip_mreqn imreq {};
    imreq.imr_address.s_addr = ESPHOME_INADDR_ANY;
    imreq.imr_ifindex = 0;
#else
    struct ip_mreq imreq {};
    imreq.imr_interface.s_addr = ESPHOME_INADDR_ANY;
#endif
    imreq.imr_multiaddr = ipv4mc;
    if (sock->setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq)) < 0) {
      return false;
    }
    if (if_index_out != nullptr) {
      *if_index_out = 0;
    }
    return true;
  }
#if USE_NETWORK_IPV6
  struct ipv6_mreq imreq6 {};
#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
#ifdef USE_ZEPHYR
  if (zsock_inet_pton(AF_INET6, ip_address, &imreq6.ipv6mr_multiaddr) != 1) {
#else
  if (inet_pton(AF_INET6, ip_address, &imreq6.ipv6mr_multiaddr) != 1) {
#endif
    errno = EINVAL;
    return false;
  }
#else
  ip6_addr_t ip6;
  if (!inet6_aton(ip_address, &ip6)) {
    errno = EINVAL;
    return false;
  }
  memcpy(imreq6.ipv6mr_multiaddr.un.u32_addr, ip6.addr, sizeof(ip6.addr));
#endif
  // POSIX: interface=0 fails for link-local multicast (ff02::) and may select loopback for
  // other scopes. LwIP: NETIF_NO_INDEX=0 is always rejected.
  bool joined = false;
  foreach_eligible_ipv6_if([&](unsigned int idx) {
#ifdef USE_ZEPHYR
    // This Zephyr fork's struct ipv6_mreq has no "ipv6mr_interface" (POSIX name) and no
    // IPV6_JOIN_GROUP alias -- only the RFC 3678 legacy ipv6mr_ifindex/IPV6_ADD_MEMBERSHIP names.
    imreq6.ipv6mr_ifindex = idx;
    if (sock->setsockopt(IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &imreq6, sizeof(imreq6)) == 0) {
#else
    imreq6.ipv6mr_interface = idx;
    if (sock->setsockopt(IPPROTO_IPV6, IPV6_JOIN_GROUP, &imreq6, sizeof(imreq6)) == 0) {
#endif
      joined = true;
      return true;
    }
    return false;
  });
  if (!joined) {
    return false;
  }
  if (if_index_out != nullptr) {
#ifdef USE_ZEPHYR
    *if_index_out = imreq6.ipv6mr_ifindex;
#else
    *if_index_out = imreq6.ipv6mr_interface;
#endif
  }
  return true;
#else
  errno = EINVAL;
  return false;
#endif  // USE_NETWORK_IPV6
}

#if USE_NETWORK_IPV6
bool set_ipv6_multicast_if(Socket *sock, uint32_t if_index_in) {
#ifdef USE_ZEPHYR
  // This Zephyr fork has no IPV6_MULTICAST_IF setsockopt at all. That's not a gap to work
  // around: nRF52 is a Thread/OpenThread device with exactly one IPv6-capable interface, so
  // there is nothing to select between -- whatever the stack already sends on is "the"
  // multicast interface. Treat this as trivially already satisfied.
  (void) sock;
  (void) if_index_in;
  return true;
#else
  uint32_t ifindex = if_index_in;
  if (ifindex == 0) {
    foreach_eligible_ipv6_if([&](unsigned int idx) {
      ifindex = idx;
      return true;
    });
    if (ifindex == 0) {
      return false;
    }
  }
  return sock->setsockopt(IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)) == 0;
#endif
}
#endif  // USE_NETWORK_IPV6
#endif  // USE_SOCKET_IMPL_BSD_SOCKETS || USE_SOCKET_IMPL_LWIP_SOCKETS

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
