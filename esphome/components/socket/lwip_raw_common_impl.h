#pragma once
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <cerrno>
#include <cstring>

#include "headers.h"
#include "lwip/ip.h"

namespace esphome::socket {

// ---- LWIP thread safety ----
//
// On RP2040 (Pico W), arduino-pico sets PICO_CYW43_ARCH_THREADSAFE_BACKGROUND=1.
// This means lwip callbacks (recv_fn, accept_fn, err_fn) run from a low-priority
// user IRQ context, not the main loop (see low_priority_irq_handler() in pico-sdk
// async_context_threadsafe_background.c). They can preempt main-loop code at any point.
//
// Without locking, this causes race conditions between recv_fn and read() on the
// shared rx_buf_ pbuf chain — recv_fn calls pbuf_cat() while read() is freeing
// nodes, leading to use-after-free and infinite-loop crashes. See esphome#10681.
//
// On ESP8266, lwip callbacks run from the SYS context which cooperates with user
// code (CONT context) — they never preempt each other, so no locking is needed.
//
// esphome::LwIPLock is the platform-provided RAII guard (see helpers.h/helpers.cpp).
// On RP2040, it acquires cyw43_arch_lwip_begin/end (WiFi) or ethernet_arch_lwip_begin/end
// (Ethernet). On ESP8266, it's a no-op.
//
// Each .cpp file that needs locking defines its own LWIP_LOCK() macro:
//   #define LWIP_LOCK() esphome::LwIPLock lwip_lock_guard
// This is a per-TU convenience macro, not defined here to avoid macro leaking.

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
