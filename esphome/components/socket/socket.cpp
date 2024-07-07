#include "socket.h"
#include <cerrno>
#include <cstring>
#include <string>
#include "esphome/core/log.h"

namespace esphome {
namespace socket {

Socket::~Socket() {}

std::unique_ptr<Socket> socket_ip(int type, int protocol) {
#if USE_NETWORK_IPV6
  return socket(AF_INET6, type, protocol);
#else
  return socket(AF_INET, type, protocol);
#endif /* USE_NETWORK_IPV6 */
}

socklen_t set_sockaddr(struct sockaddr *addr, socklen_t addrlen, const std::string &ip_address, uint16_t port) {
#if USE_NETWORK_IPV6
  if (addrlen < sizeof(sockaddr_in6)) {
    errno = EINVAL;
    return 0;
  }
  auto *server = reinterpret_cast<sockaddr_in6 *>(addr);
  memset(server, 0, sizeof(sockaddr_in6));
  server->sin6_family = AF_INET6;
  server->sin6_port = htons(port);

  if (ip_address.find('.') != std::string::npos) {
    server->sin6_addr.un.u32_addr[3] = inet_addr(ip_address.c_str());
  } else {
    ip6_addr_t ip6;
    inet6_aton(ip_address.c_str(), &ip6);
    memcpy(server->sin6_addr.un.u32_addr, ip6.addr, sizeof(ip6.addr));
  }
  return sizeof(sockaddr_in6);
#else
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
#endif /* USE_NETWORK_IPV6 */
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
