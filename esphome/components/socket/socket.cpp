#include "socket.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace socket {

std::unique_ptr<Socket> socket_ip(int type, int protocol) {
#if LWIP_IPV6
  return socket(AF_INET6, type, protocol);
#else
  return socket(AF_INET, type, protocol);
#endif
}

void set_sockaddr_any(struct sockaddr *addr, uint16_t port) {
#if LWIP_IPV6
  auto *server = reinterpret_cast<sockaddr_in6 *>(addr);
  memset(server, 0, sizeof(sockaddr_in6));
  server->sin6_family = AF_INET6;
  server->sin6_port = port;
#else
  auto *server = reinterpret_cast<sockaddr_in *>(addr);
  memset(server, 0, sizeof(sockaddr_in));
  server->sin_family = AF_INET;
  server->sin_addr.s_addr = ESPHOME_INADDR_ANY;
  server->sin_port = port;
#endif
}
}  // namespace socket
}  // namespace esphome
