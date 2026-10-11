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

bool Ipv4Resolve::consume_failure() {
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
  uint8_t expected = STATE_FAILED;
  return this->state_word_.compare_exchange_strong(expected, STATE_IDLE);
#else
  if (this->state_word_ != STATE_FAILED) {
    return false;
  }
  this->state_word_ = STATE_IDLE;
  return true;
#endif
}

void Ipv4Resolve::forget() {
#if defined(IPV4_RESOLVE_VOLATILE_EPOCH)
  // The generation moves first; RESOLVING stays set until the callback
  // drops the stale result, so start() cannot replace that lookup.
  this->epoch_ = this->epoch_ + 1;
  this->addr_word_ = 0;
  if (this->state_word_ != STATE_RESOLVING) {
    this->state_word_ = STATE_IDLE;
  }
#elif defined(IPV4_RESOLVE_ATOMIC_STATE)
  // PUBLISHING stays, so start() cannot queue a second callback
  // while this one is storing the address.
  uint8_t expected = STATE_RESOLVING;
  if (this->state_word_.compare_exchange_strong(expected, STATE_IDLE)) {
    this->set_addr_(0);
    return;
  }
  if (this->state_() == STATE_PUBLISHING) {
    return;
  }
  this->set_state_(STATE_IDLE);
  this->set_addr_(0);
#else
  this->set_state_(STATE_IDLE);
  this->set_addr_(0);
#endif
}

socklen_t Ipv4Resolve::to_sockaddr(struct sockaddr *dest, socklen_t destlen, uint16_t port) const {
  if (this->state_() != STATE_RESOLVED || destlen < sizeof(sockaddr_in)) {
    return 0;
  }
  auto *in = reinterpret_cast<sockaddr_in *>(dest);
  memset(in, 0, sizeof(sockaddr_in));
  in->sin_family = AF_INET;
  in->sin_port = htons(port);
  in->sin_addr.s_addr = this->addr_();
  return sizeof(sockaddr_in);
}

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#if defined(IPV4_RESOLVE_VOLATILE_EPOCH)
bool Ipv4Resolve::drop_stale_(uint32_t expected) {
  if (this->epoch_ == expected) {
    return false;
  }
  this->state_word_ = STATE_IDLE;
  this->addr_word_ = 0;
  return true;
}
#endif

void Ipv4Resolve::dns_found(const char *name, const ip_addr_t *addr, void *arg) {
  auto *self = static_cast<Ipv4Resolve *>(arg);
#if defined(IPV4_RESOLVE_VOLATILE_EPOCH)
  const uint32_t expected = self->pending_epoch_;
  if (self->drop_stale_(expected)) {
    return;
  }
#endif
  if (addr != nullptr && IP_IS_V4(addr)) {
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
    // Only the callback that wins the exchange owns the lookup.
    uint8_t expected = STATE_RESOLVING;
    if (!self->state_word_.compare_exchange_strong(expected, STATE_PUBLISHING)) {
      return;
    }
    self->set_addr_(ip4_addr_get_u32(ip_2_ip4(addr)));
    expected = STATE_PUBLISHING;
    // Lost ownership; the leftover address is gated by to_sockaddr()'s state check.
    if (!self->state_word_.compare_exchange_strong(expected, STATE_RESOLVED)) {
      return;
    }
#elif defined(IPV4_RESOLVE_VOLATILE_EPOCH)
    self->addr_word_ = ip4_addr_get_u32(ip_2_ip4(addr));
    self->state_word_ = STATE_RESOLVED;
    if (self->drop_stale_(expected)) {
      return;
    }
#else
    // Do not publish over a newer start().
    if (self->state_() != STATE_RESOLVING) {
      return;
    }
    self->set_addr_(ip4_addr_get_u32(ip_2_ip4(addr)));
    self->set_state_(STATE_RESOLVED);
#endif
  } else {
    ESP_LOGW(self->tag_ != nullptr ? self->tag_ : TAG, "DNS failed for %s", name);
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
    uint8_t expected = STATE_RESOLVING;
    self->state_word_.compare_exchange_strong(expected, STATE_FAILED);
#elif defined(IPV4_RESOLVE_VOLATILE_EPOCH)
    self->state_word_ = STATE_FAILED;
    if (self->drop_stale_(expected)) {
      return;
    }
#else
    if (self->state_() != STATE_RESOLVING) {
      return;
    }
    self->set_state_(STATE_FAILED);
#endif
  }
}
#endif

void Ipv4Resolve::start(const char *host, uint16_t port, const char *tag) {
  const uint8_t state = this->state_();
  if (state == STATE_RESOLVED || state == STATE_RESOLVING || state == STATE_PUBLISHING) {
    return;
  }
  this->set_state_(STATE_IDLE);
  this->tag_ = tag;
  struct sockaddr_storage literal;
  if (set_sockaddr(reinterpret_cast<struct sockaddr *>(&literal), sizeof(literal), host, port) != 0) {
    if (literal.ss_family == AF_INET) {
      auto *in = reinterpret_cast<sockaddr_in *>(&literal);
      this->set_addr_(in->sin_addr.s_addr);
      this->set_state_(STATE_RESOLVED);
      return;
    }
    this->set_state_(STATE_FAILED);
    ESP_LOGW(tag, "Not an IPv4 address: %s", host);
    return;
  }
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  ip_addr_t cached;
  err_t err;
  {
    LwIPLock lock;
#if defined(IPV4_RESOLVE_VOLATILE_EPOCH)
    this->pending_epoch_ = this->epoch_;
#endif
    this->set_state_(STATE_RESOLVING);
    err = dns_gethostbyname_addrtype(host, &cached, &Ipv4Resolve::dns_found, this, LWIP_DNS_ADDRTYPE_IPV4);
    if (err != ERR_INPROGRESS && this->state_() == STATE_RESOLVING) {
      this->set_state_(STATE_IDLE);
    }
  }
  if (err == ERR_OK && IP_IS_V4(&cached)) {
    this->set_addr_(ip4_addr_get_u32(ip_2_ip4(&cached)));
    this->set_state_(STATE_RESOLVED);
    return;
  }
  if (err == ERR_INPROGRESS || this->state_() == STATE_RESOLVED || this->state_() == STATE_PUBLISHING) {
    return;
  }
#else
  struct addrinfo hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &res) == 0 && res != nullptr) {
    auto *in = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    if (res->ai_family == AF_INET) {
      this->set_addr_(in->sin_addr.s_addr);
      this->set_state_(STATE_RESOLVED);
    }
    freeaddrinfo(res);
    if (this->ready()) {
      return;
    }
  }
#endif
  this->set_state_(STATE_FAILED);
  ESP_LOGW(tag, "Could not resolve %s", host);
}

}  // namespace esphome::socket

#endif
