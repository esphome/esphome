#pragma once
#include <memory>
#include <span>
#include <string>

#include "esphome/core/optional.h"
#include "headers.h"

#ifdef USE_LWIP_FAST_SELECT
#include "esphome/core/lwip_fast_select.h"
#endif

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

// Include only the active implementation's header.
// SOCKADDR_STR_LEN is defined in headers.h.
#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
#include "bsd_sockets_impl.h"
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "lwip_sockets_impl.h"
#elif defined(USE_SOCKET_IMPL_LWIP_TCP)
#include "lwip_raw_tcp_impl.h"
#endif

namespace esphome::socket {

// Type aliases — only one implementation is active per build.
// Socket is the concrete type for connected sockets.
// ListenSocket is the concrete type for listening/server sockets.
// On BSD and LWIP_SOCKETS, both aliases resolve to the same type.
// On LWIP_TCP, they are different types (no virtual dispatch between them).
#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
using Socket = BSDSocketImpl;
using ListenSocket = BSDSocketImpl;
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
using Socket = LwIPSocketImpl;
using ListenSocket = LwIPSocketImpl;
#elif defined(USE_SOCKET_IMPL_LWIP_TCP)
using Socket = LWIPRawImpl;
using ListenSocket = LWIPRawListenImpl;
#endif

#ifdef USE_LWIP_FAST_SELECT
/// Shared ready() helper using cached lwip_sock pointer for direct rcvevent read.
inline bool socket_ready(struct lwip_sock *cached_sock, bool loop_monitored) {
  return !loop_monitored || (cached_sock != nullptr && esphome_lwip_socket_has_data(cached_sock));
}
#elif defined(USE_SOCKET_SELECT_SUPPORT)
/// Shared ready() helper for fd-based socket implementations.
/// Checks if the Application's select() loop has marked this fd as ready.
bool socket_ready_fd(int fd, bool loop_monitored);
#endif

// Inline ready() — defined here because it depends on socket_ready/socket_ready_fd
// declared above, while the impl headers are included before those declarations.
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
inline bool Socket::ready() const {
#ifdef USE_LWIP_FAST_SELECT
  return socket_ready(this->cached_sock_, this->loop_monitored_);
#else
  return socket_ready_fd(this->fd_, this->loop_monitored_);
#endif
}
#endif

/// Create a socket of the given domain, type and protocol.
std::unique_ptr<Socket> socket(int domain, int type, int protocol);
/// Create a socket in the newest available IP domain (IPv6 or IPv4) of the given type and protocol.
std::unique_ptr<Socket> socket_ip(int type, int protocol);

/// Create a socket and monitor it for data in the main loop.
/// Like socket() but also registers the socket with the Application's select() loop.
/// WARNING: These functions are NOT thread-safe. They must only be called from the main loop
/// as they register the socket file descriptor with the global Application instance.
/// NOTE: On ESP platforms, FD_SETSIZE is typically 10, limiting the number of monitored sockets.
/// File descriptors >= FD_SETSIZE will not be monitored and will log an error.
std::unique_ptr<Socket> socket_loop_monitored(int domain, int type, int protocol);

/// Create a listening socket of the given domain, type and protocol.
/// Create a listening socket and monitor it for data in the main loop.
/// Create a listening socket in the newest available IP domain and monitor it.
#ifdef USE_SOCKET_IMPL_LWIP_TCP
// LWIP_TCP has separate Socket/ListenSocket types — needs distinct factory functions.
std::unique_ptr<ListenSocket> socket_listen(int domain, int type, int protocol);
std::unique_ptr<ListenSocket> socket_listen_loop_monitored(int domain, int type, int protocol);
std::unique_ptr<ListenSocket> socket_ip_loop_monitored(int type, int protocol);
#else
// BSD and LWIP_SOCKETS: Socket == ListenSocket, so listen variants just delegate.
inline std::unique_ptr<ListenSocket> socket_listen(int domain, int type, int protocol) {
  return socket(domain, type, protocol);
}
inline std::unique_ptr<ListenSocket> socket_listen_loop_monitored(int domain, int type, int protocol) {
  return socket_loop_monitored(domain, type, protocol);
}
inline std::unique_ptr<ListenSocket> socket_ip_loop_monitored(int type, int protocol) {
#if USE_NETWORK_IPV6
  return socket_loop_monitored(AF_INET6, type, protocol);
#else
  return socket_loop_monitored(AF_INET, type, protocol);
#endif
}
#endif

/// Set a sockaddr to the specified address and port for the IP version used by socket_ip().
/// @param addr Destination sockaddr structure
/// @param addrlen Size of the addr buffer
/// @param ip_address Null-terminated IP address string (IPv4 or IPv6)
/// @param port Port number in host byte order
/// @return Size of the sockaddr structure used, or 0 on error
socklen_t set_sockaddr(struct sockaddr *addr, socklen_t addrlen, const char *ip_address, uint16_t port);

/// Convenience overload for std::string (backward compatible).
inline socklen_t set_sockaddr(struct sockaddr *addr, socklen_t addrlen, const std::string &ip_address, uint16_t port) {
  return set_sockaddr(addr, addrlen, ip_address.c_str(), port);
}

/// Set a sockaddr to the any address and specified port for the IP version used by socket_ip().
socklen_t set_sockaddr_any(struct sockaddr *addr, socklen_t addrlen, uint16_t port);

/// Format sockaddr into caller-provided buffer, returns length written (excluding null)
size_t format_sockaddr_to(const struct sockaddr *addr_ptr, socklen_t len, std::span<char, SOCKADDR_STR_LEN> buf);

#if (defined(USE_ESP8266) || defined(USE_RP2040)) && defined(USE_SOCKET_IMPL_LWIP_TCP)
/// Delay that can be woken early by socket activity.
/// On ESP8266, uses esp_delay() with a callback that checks socket activity.
/// On RP2040, uses __wfe() (Wait For Event) to truly sleep until an interrupt
/// (for example, CYW43 GPIO or a timer alarm) fires and wakes the CPU.
void socket_delay(uint32_t ms);

/// Signal socket/IO activity and wake the main loop early.
/// On ESP8266: sets flag + esp_schedule().
/// On RP2040: sets flag + __sev() (Send Event) to wake from __wfe().
/// ISR-safe on both platforms.
void socket_wake();  // NOLINT(readability-redundant-declaration)
#endif

}  // namespace esphome::socket
#endif
