#include "socket.h"
#include "esphome/core/log.h"
#include <cstring>
#include <cerrno>

namespace esphome {
namespace socket {

std::unique_ptr<Socket> socket_ip(int type, int protocol) {
#if LWIP_IPV6
  return socket(AF_INET6, type, protocol);
#else
  return socket(AF_INET, type, protocol);
#endif
}

socklen_t set_sockaddr_any(struct sockaddr *addr, socklen_t addrlen, uint16_t port) {
#if LWIP_IPV6
  if (addrlen < sizeof(sockaddr_in6)) {
    errno = EINVAL;
    return 0;
  }
  auto *server = reinterpret_cast<sockaddr_in6 *>(addr);
  memset(server, 0, sizeof(sockaddr_in6));
  server->sin6_family = AF_INET6;
  server->sin6_port = port;
  server->sin6_addr = in6addr_any;
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
  server->sin_port = port;
  return sizeof(sockaddr_in);
#endif
}
}  // namespace socket
}  // namespace esphome
