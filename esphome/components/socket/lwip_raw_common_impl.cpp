#include "lwip_raw_common_impl.h"
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <cerrno>
#include <cstring>

namespace esphome::socket {

int lwip_ip_to_sockaddr(sa_family_t family, const ip_addr_t *ip, uint16_t port_host, struct sockaddr *name,
                        socklen_t *addrlen) {
  if (family == AF_INET) {
    if (*addrlen < sizeof(struct sockaddr_in)) {
      errno = EINVAL;
      return -1;
    }
    auto *addr = reinterpret_cast<struct sockaddr_in *>(name);
    addr->sin_family = AF_INET;
    *addrlen = addr->sin_len = sizeof(struct sockaddr_in);
    addr->sin_port = htons(port_host);
    inet_addr_from_ip4addr(&addr->sin_addr, ip_2_ip4(ip));
    return 0;
  }
#if LWIP_IPV6
  if (family == AF_INET6) {
    if (*addrlen < sizeof(struct sockaddr_in6)) {
      errno = EINVAL;
      return -1;
    }
    auto *addr = reinterpret_cast<struct sockaddr_in6 *>(name);
    addr->sin6_family = AF_INET6;
    *addrlen = addr->sin6_len = sizeof(struct sockaddr_in6);
    addr->sin6_port = htons(port_host);
    // AF_INET6 sockets may receive IPv4 packets; convert to IPv4-mapped IPv6.
    if (IP_IS_V4(ip)) {
      ip_addr_t mapped;
      ip4_2_ipv4_mapped_ipv6(ip_2_ip6(&mapped), ip_2_ip4(ip));
      inet6_addr_from_ip6addr(&addr->sin6_addr, ip_2_ip6(&mapped));
    } else {
      inet6_addr_from_ip6addr(&addr->sin6_addr, ip_2_ip6(ip));
    }
    return 0;
  }
#endif
  return -1;
}

bool sockaddr_to_lwip(const struct sockaddr *addr, socklen_t addrlen, ip_addr_t *ip, uint16_t *port) {
  if (addrlen < sizeof(struct sockaddr))
    return false;
#if LWIP_IPV6
  if (addr->sa_family == AF_INET) {
    if (addrlen < sizeof(sockaddr_in))
      return false;
    auto *addr4 = reinterpret_cast<const sockaddr_in *>(addr);
    *port = ntohs(addr4->sin_port);
    ip->type = IPADDR_TYPE_V4;
    ip->u_addr.ip4.addr = addr4->sin_addr.s_addr;
    return true;
  }
  if (addr->sa_family == AF_INET6) {
    if (addrlen < sizeof(sockaddr_in6))
      return false;
    auto *addr6 = reinterpret_cast<const sockaddr_in6 *>(addr);
    *port = ntohs(addr6->sin6_port);
    ip->type = IPADDR_TYPE_V6;
    memcpy(&ip->u_addr.ip6.addr, &addr6->sin6_addr.un.u8_addr, 16);
    return true;
  }
#else
  if (addr->sa_family == AF_INET) {
    if (addrlen < sizeof(sockaddr_in))
      return false;
    auto *addr4 = reinterpret_cast<const sockaddr_in *>(addr);
    *port = ntohs(addr4->sin_port);
    ip->addr = addr4->sin_addr.s_addr;
    return true;
  }
#endif
  return false;
}

int lwip_bind_err(err_t err) {
  if (err == ERR_OK)
    return 0;
  if (err == ERR_USE) {
    errno = EADDRINUSE;
  } else if (err == ERR_VAL) {
    errno = EINVAL;
  } else {
    errno = EIO;
  }
  return -1;
}

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
