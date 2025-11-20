#include "socket.h"
#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)
#include <cerrno>
#include <cstring>
#include <string>
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace socket {

Socket::~Socket() {}

bool Socket::ready() const {
#ifdef USE_SOCKET_SELECT_SUPPORT
  if (!loop_monitored_) {
    // Non-monitored sockets always return true (assume data may be available)
    return true;
  }

  // For loop-monitored sockets, check with the Application's select() results
  int fd = this->get_fd();
  if (fd < 0) {
    // No valid file descriptor, assume ready (fallback behavior)
    return true;
  }

  return App.is_socket_ready(fd);
#else
  // Without select() support, we can't monitor sockets in the loop
  // Always return true (assume data may be available)
  return true;
#endif
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
}  // namespace socket
}  // namespace esphome
#endif
