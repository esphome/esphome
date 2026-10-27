#include "socket.h"
#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)
#include <cerrno>
#include <cstring>
#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
#include <sys/select.h>
#endif
#include <string>
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#ifdef USE_HOST
#include "esphome/core/wake.h"
#endif
#if defined(USE_HOST)
#include <ifaddrs.h>
#include <net/if.h>
#elif defined(USE_ZEPHYR)
#ifndef CLANG_TIDY  // NCS 2.9.2 net_if.h has a BUILD_ASSERT clang rejects; gone in NCS 3.4
#include <zephyr/net/net_if.h>
#endif
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
#if USE_NETWORK_IPV4
#if defined(USE_SOCKET_IMPL_LWIP_TCP)
// LWIP raw TCP (ESP8266) uses inet_ntoa_r which takes struct by value
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  inet_ntoa_r(*reinterpret_cast<const struct in_addr *>(addr), buf, size);
  return buf;
}
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
// LWIP sockets (LibreTiny, ESP32 Arduino)
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  return lwip_inet_ntop(AF_INET, addr, buf, size);
}
#elif defined(USE_ZEPHYR)
// Zephyr BSD sockets — use Zephyr native address formatting via POSIX-subset wrappers.
// <zephyr/net/socket.h> is already included transitively through <sys/socket.h>.
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  return zsock_inet_ntop(AF_INET, addr, buf, size);
}
#else
// BSD sockets (host, ESP32-IDF)
static inline const char *esphome_inet_ntop4(const void *addr, char *buf, size_t size) {
  return inet_ntop(AF_INET, addr, buf, size);
}
#endif
#endif /* USE_NETWORK_IPV4 */

#if USE_NETWORK_IPV6
#if defined(USE_SOCKET_IMPL_LWIP_TCP)
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  inet6_ntoa_r(*reinterpret_cast<const ip6_addr_t *>(addr), buf, size);
  return buf;
}
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  return lwip_inet_ntop(AF_INET6, addr, buf, size);
}
#elif defined(USE_ZEPHYR)
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  return zsock_inet_ntop(AF_INET6, addr, buf, size);
}
#else
static inline const char *esphome_inet_ntop6(const void *addr, char *buf, size_t size) {
  return inet_ntop(AF_INET6, addr, buf, size);
}
#endif
#endif /* USE_NETWORK_IPV6 */

bool sockaddr_to_ipv4(const struct sockaddr *addr, uint32_t *out) {
#if USE_NETWORK_IPV4
  if (addr->sa_family == AF_INET) {
    *out = reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr.s_addr;
    return true;
  }
#if USE_NETWORK_IPV6
  if (addr->sa_family == AF_INET6) {
    // ::ffff:a.b.c.d; s6_addr is the portable byte view on every stack.
    static constexpr uint8_t V4_MAPPED_PREFIX[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    const uint8_t *bytes = reinterpret_cast<const struct sockaddr_in6 *>(addr)->sin6_addr.s6_addr;
    if (memcmp(bytes, V4_MAPPED_PREFIX, sizeof(V4_MAPPED_PREFIX)) == 0) {
      memcpy(out, bytes + sizeof(V4_MAPPED_PREFIX), sizeof(*out));
      return true;
    }
  }
#endif
#else
  (void) addr;
  (void) out;
#endif /* USE_NETWORK_IPV4 */
  return false;
}

// Format sockaddr into caller-provided buffer, returns length written (excluding null)
size_t format_sockaddr_to(const struct sockaddr *addr_ptr, socklen_t len, std::span<char, SOCKADDR_STR_LEN> buf) {
#if USE_NETWORK_IPV4
  if (addr_ptr->sa_family == AF_INET && len >= sizeof(const struct sockaddr_in)) {
    const auto *addr = reinterpret_cast<const struct sockaddr_in *>(addr_ptr);
    if (esphome_inet_ntop4(&addr->sin_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
  }
#endif /* USE_NETWORK_IPV4 */
#if USE_NETWORK_IPV6
  if (addr_ptr->sa_family == AF_INET6 && len >= sizeof(sockaddr_in6)) {
    const auto *addr = reinterpret_cast<const struct sockaddr_in6 *>(addr_ptr);
#if USE_NETWORK_IPV4
    uint32_t v4;
    // Format a v4 mapped peer as plain IPv4.
    if (sockaddr_to_ipv4(addr_ptr, &v4) && esphome_inet_ntop4(&v4, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
#endif /* USE_NETWORK_IPV4 */
    if (esphome_inet_ntop6(&addr->sin6_addr, buf.data(), buf.size()) != nullptr)
      return strlen(buf.data());
  }
#endif
  buf[0] = '\0';
  return 0;
}

std::unique_ptr<Socket> socket_ip(int type, int protocol) { return socket(IP_DOMAIN, type, protocol); }

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
// Callback must be a temporary lambda
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
#elif defined(USE_ZEPHYR) && defined(CLANG_TIDY)
  (void) callback;
  return false;
#elif defined(USE_ZEPHYR)
  // Zephyr has no getifaddrs()/ifaddrs.h; enumerate interfaces via the native net_if API.
  // NET_IF_IPV6_NO_MLD is the direct analog of the LwIP branch's NETIF_FLAG_MLD6 check below.
  struct EligibleIfContext {
    F *callback;
    bool found_any = false;
    bool done = false;
  };
  EligibleIfContext ctx{&callback};
  net_if_foreach(
      [](struct net_if *iface, void *user_data) {
        auto *ctx = static_cast<EligibleIfContext *>(user_data);
        if (ctx->done) {
          return;
        }
        if (!net_if_flag_is_set(iface, NET_IF_UP) || !net_if_flag_is_set(iface, NET_IF_IPV6) ||
            net_if_flag_is_set(iface, NET_IF_IPV6_NO_MLD)) {
          return;
        }
        int idx = net_if_get_by_iface(iface);
        if (idx <= 0) {
          return;
        }
        ctx->found_any = true;
        if ((*ctx->callback)(static_cast<unsigned int>(idx))) {
          ctx->done = true;
        }
      },
      &ctx);
  if (!ctx.found_any) {
    errno = EADDRNOTAVAIL;
  }
  return ctx.found_any;
#else
  // The netif list needs the core lock, but the callback's setsockopt() takes that lock itself, so the
  // indices are collected first. One that goes away before the callback runs is rejected by setsockopt().
  // Devices have only a few interfaces, so the cap drops nothing in practice.
  static constexpr size_t MAX_IFS = 8;
  StaticVector<uint8_t, MAX_IFS> indices;
  {
    LwIPLock lock;
    struct netif *netif;
    NETIF_FOREACH(netif) {
      if (indices.size() == MAX_IFS) {
        break;
      }
      if (netif->name[0] == 'l' && netif->name[1] == 'o') {
        continue;
      }
      if (!(netif->flags & NETIF_FLAG_UP) || !(netif->flags & NETIF_FLAG_MLD6)) {
        continue;
      }
      indices.push_back(netif_get_index(netif));
    }
  }
  for (uint8_t index : indices) {
    if (callback(index)) {
      break;
    }
  }
  if (indices.empty()) {
    errno = EADDRNOTAVAIL;
  }
  return !indices.empty();
#endif
}
#endif  // USE_SOCKET_IMPL_BSD_SOCKETS || USE_SOCKET_IMPL_LWIP_SOCKETS

bool join_multicast_group(UDPSocket *sock, const char *ip_address, uint32_t *if_index_out) {
  if (strchr(ip_address, ':') == nullptr) {
#if !USE_NETWORK_IPV4
    errno = EAFNOSUPPORT;
    return false;
#else
    struct in_addr ipv4mc{};
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
    struct ip_mreqn imreq{};
    imreq.imr_address.s_addr = ESPHOME_INADDR_ANY;
    imreq.imr_ifindex = 0;
#else
    struct ip_mreq imreq{};
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
#endif  // USE_NETWORK_IPV4
  }
#ifdef USE_SOCKET_IPV6
  struct ipv6_mreq imreq6{};
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
#endif  // USE_SOCKET_IPV6
}

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#if USE_NETWORK_IPV6
bool set_ipv6_multicast_if(Socket *sock, uint32_t if_index_in) {
#if defined(USE_ZEPHYR) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  // Upstream lwIP (Zephyr's fork, LibreTiny) has no IPV6_MULTICAST_IF; outgoing
  // multicast follows the default netif. Zephyr gains ZSOCK_IPV6_MULTICAST_IF in NCS 3.4.0.
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
      errno = EADDRNOTAVAIL;
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
    if (inet6_aton(ip_address, &ip6) != 1) {
      errno = EINVAL;
      return 0;
    }
    memcpy(server->sin6_addr.un.u32_addr, ip6.addr, sizeof(ip6.addr));
#endif
    return sizeof(sockaddr_in6);
  }
#endif /* USE_NETWORK_IPV6 */
#if USE_NETWORK_IPV4
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
  // inet_aton() reports failure separately from the address, so 255.255.255.255 needs no special case
  if (inet_aton(ip_address, &server->sin_addr) == 0) {
    errno = EINVAL;
    return 0;
  }
#endif
  server->sin_port = htons(port);
  return sizeof(sockaddr_in);
#else
  memset(addr, 0, addrlen);
  errno = EAFNOSUPPORT;
  return 0;
#endif /* USE_NETWORK_IPV4 */
}

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
ConnectPollResult poll_connect(Socket &sock, int &err_out) {
  int fd = sock.get_fd();
  if (fd < 0 || fd >= FD_SETSIZE) {
    // FD_SET on either is undefined behavior
    err_out = EBADF;
    return ConnectPollResult::CONNECT_POLL_RESULT_ERROR;
  }
  // Connect completion is a write event; the main loop only selects on reads
  fd_set writefds;
  FD_ZERO(&writefds);
  FD_SET(fd, &writefds);
  struct timeval tv = {0, 0};
#ifdef USE_SOCKET_IMPL_LWIP_SOCKETS
  // LWIP_COMPAT_SOCKETS may be off (LibreTiny), so use the lwip symbol directly
  int ret = lwip_select(fd + 1, nullptr, &writefds, nullptr, &tv);
#else
  // Global-scope select: the entity namespace esphome::select shadows it here
  int ret = ::select(fd + 1, nullptr, &writefds, nullptr, &tv);
#endif
  if (ret < 0) {
    err_out = errno;
    return ConnectPollResult::CONNECT_POLL_RESULT_ERROR;
  }
  if (ret == 0) {
    return ConnectPollResult::CONNECT_POLL_RESULT_PENDING;
  }
  int error = 0;
  socklen_t len = sizeof(error);
  if (sock.getsockopt(SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
    err_out = errno;
    return ConnectPollResult::CONNECT_POLL_RESULT_ERROR;
  }
  if (error != 0) {
    err_out = error;
    return ConnectPollResult::CONNECT_POLL_RESULT_ERROR;
  }
  return ConnectPollResult::CONNECT_POLL_RESULT_CONNECTED;
}
#endif

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
