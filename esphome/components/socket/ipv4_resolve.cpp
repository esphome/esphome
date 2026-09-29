#include "ipv4_resolve.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "socket.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstdio>
#include <cstring>

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace esphome::socket {

static const char *const TAG = "socket";

bool Ipv4Resolve::ready() {
  if (this->text_[0] != '\0') {
    return true;
  }
  if (!this->have_.load()) {
    return false;
  }
#if defined(USE_HOST) || defined(USE_ZEPHYR)
  return false;
#else
  ip4_addr_t addr;
  ip4_addr_set_u32(&addr, this->addr_.load());
  ip4addr_ntoa_r(&addr, this->text_, static_cast<int>(sizeof(this->text_)));
  return this->text_[0] != '\0';
#endif
}

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
void Ipv4Resolve::dns_found_(const char *name, const ip_addr_t *addr, void *arg) {
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
    snprintf(this->text_, sizeof(this->text_), "%s", host);
    this->have_.store(true);
    return;
  }
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  ip_addr_t cached;
  err_t err;
  {
    LwIPLock lock;
    this->resolving_.store(true);
    err = dns_gethostbyname(host, &cached, &Ipv4Resolve::dns_found_, this);
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
  struct addrinfo hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &res) == 0 && res != nullptr) {
    char buf[SOCKADDR_STR_LEN];
    auto *in = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf)) != nullptr) {
      snprintf(this->text_, sizeof(this->text_), "%s", buf);
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
