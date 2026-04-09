#pragma once
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <cerrno>
#include <cstring>

#include "headers.h"
#include "lwip/ip.h"

namespace esphome::socket {

/// Convert lwip ip_addr_t + host-order port to sockaddr, based on the socket's address family.
/// @param port_host Port in host byte order. TCP callers must convert from network order first
///                  (tcp_pcb stores ports in network byte order); UDP callers can pass directly
///                  (lwip udp_recv callback provides port in host byte order).
/// Shared by both TCP (LWIPRawCommon) and UDP (LWIPRawUDPImpl) implementations.
int lwip_ip_to_sockaddr(sa_family_t family, const ip_addr_t *ip, uint16_t port_host, struct sockaddr *name,
                        socklen_t *addrlen);

/// Convert sockaddr to lwip ip_addr_t and host-order port.
/// For IPv6, sets type to IPADDR_TYPE_V6 (callers that need dual-stack should
/// override to IPADDR_TYPE_ANY after calling).
/// Shared by both TCP (LWIPRawCommon) and UDP (LWIPRawUDPImpl) bind/sendto paths.
bool sockaddr_to_lwip(const struct sockaddr *addr, socklen_t addrlen, ip_addr_t *ip, uint16_t *port);

/// Map lwip bind error to errno. Returns 0 on success, -1 on error with errno set.
int lwip_bind_err(err_t err);

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
