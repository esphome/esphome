#include "socket.h"
#include <cerrno>
#include <cstring>
#include <string>
#include "esphome/core/log.h"

namespace esphome {
namespace socket_shd {

Socket::~Socket() {}

std::unique_ptr<Socket> socket_ip(int type, int protocol) {
#if LWIP_IPV6
  return socket(AF_INET6, type, protocol);
#else
  return socket(AF_INET, type, protocol);
#endif
}

}  // namespace socket_shd
}  // namespace esphome
