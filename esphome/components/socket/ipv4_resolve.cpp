#include "ipv4_resolve.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "socket.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#include "lwip/dns.h"
#else
#include <netdb.h>
#endif

namespace esphome::socket {

static const char *const TAG = "socket";

socklen_t Ipv4Resolve::to_sockaddr(struct sockaddr *dest, socklen_t destlen, uint16_t port) const {
  if (!this->have_.load() || destlen < sizeof(sockaddr_in)) {
    return 0;
  }
  auto *in = reinterpret_cast<sockaddr_in *>(dest);
  memset(in, 0, sizeof(sockaddr_in));
  in->sin_family = AF_INET;
  in->sin_port = htons(port);
  in->sin_addr.s_addr = this->addr_.load();
  return sizeof(sockaddr_in);
}

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
void Ipv4Resolve::dns_found(const char *name, const ip_addr_t *addr, void *arg) {
  auto *self = static_cast<Ipv4Resolve *>(arg);
  if (addr != nullptr && IP_IS_V4(addr)) {
    self->addr_.store(ip4_addr_get_u32(ip_2_ip4(addr)));
    self->have_.store(true);
  } else {
    self->failed_.store(1);
    ESP_LOGW(self->tag_ != nullptr ? self->tag_ : TAG, "DNS failed for %s", name);
  }
  self->resolving_.store(false);
}
#endif

void Ipv4Resolve::start(const char *host, uint16_t port, const char *tag) {
  if (this->have_.load() || this->resolving_.load()) {
    return;
  }
  this->tag_ = tag;
  struct sockaddr_storage literal;
  if (set_sockaddr(reinterpret_cast<struct sockaddr *>(&literal), sizeof(literal), host, port) != 0) {
    if (literal.ss_family == AF_INET) {
      auto *in = reinterpret_cast<sockaddr_in *>(&literal);
      this->addr_.store(in->sin_addr.s_addr);
      this->have_.store(true);
      return;
    }
    this->failed_.store(1);
    ESP_LOGW(tag, "Not an IPv4 address: %s", host);
    return;
  }
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  ip_addr_t cached;
  err_t err;
  {
    LwIPLock lock;
    this->resolving_.store(true);
    err = dns_gethostbyname(host, &cached, &Ipv4Resolve::dns_found, this);
    if (err != ERR_INPROGRESS) {
      this->resolving_.store(false);
    }
  }
  if (err == ERR_OK && IP_IS_V4(&cached)) {
    this->addr_.store(ip4_addr_get_u32(ip_2_ip4(&cached)));
    this->have_.store(true);
    return;
  }
  if (err == ERR_INPROGRESS) {
    return;
  }
#else
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &res) == 0 && res != nullptr) {
    auto *in = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    if (res->ai_family == AF_INET) {
      this->addr_.store(in->sin_addr.s_addr);
      this->have_.store(true);
    }
    freeaddrinfo(res);
    if (this->have_.load()) {
      return;
    }
  }
#endif
  this->failed_.store(1);
  ESP_LOGW(tag, "Could not resolve %s", host);
}

}  // namespace esphome::socket

#endif
