#pragma once

#include "headers.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include <cstdint>

// ESPHOME_THREAD_MULTI_ATOMICS: one atomic state word. compare_exchange closes the race.
// A callback publishes only after it has won RESOLVING -> PUBLISHING, so a second
// callback cannot clear the address.
// ESPHOME_THREAD_SINGLE: a volatile state word. The callback does not run beside loop().
// After forget() it does not publish over IDLE or RESOLVED. It can still publish if
// start() has begun another lookup.
// ESPHOME_THREAD_MULTI_NO_ATOMICS: volatile state plus a generation. BK72xx has no
// compare_exchange, and the DNS callback runs on the tcpip thread.
#if defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
#define IPV4_RESOLVE_VOLATILE_EPOCH
#elif defined(ESPHOME_THREAD_SINGLE)
#define IPV4_RESOLVE_VOLATILE
#else
#define IPV4_RESOLVE_ATOMIC_STATE
#include <atomic>
#endif

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#include "lwip/ip_addr.h"
#endif

namespace esphome::socket {

/// One IPv4 literal or hostname. The address is stored as an integer.
/// The object must outlive a pending lookup.
class Ipv4Resolve {
 public:
  static constexpr uint8_t STATE_IDLE = 0;
  static constexpr uint8_t STATE_RESOLVING = 1;
  static constexpr uint8_t STATE_RESOLVED = 2;
  static constexpr uint8_t STATE_FAILED = 3;
  // The callback holds this between winning the lookup and storing the address.
  static constexpr uint8_t STATE_PUBLISHING = 4;

  void forget();
  /// Drop a failed lookup so the next start() tries again.
  bool consume_failure();
  bool ready() const { return this->state_() == STATE_RESOLVED; }
  /// Write the stored address into dest. Returns 0 until ready() is true.
  socklen_t to_sockaddr(struct sockaddr *dest, socklen_t destlen, uint16_t port) const;
  /// Resolve host. tag is used for the failure log, including the async callback.
  /// On the host and on Zephyr this calls getaddrinfo() and blocks until it returns.
  void start(const char *host, uint16_t port, const char *tag);

 private:
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  static void dns_found(const char *name, const ip_addr_t *addr, void *arg);
#if defined(IPV4_RESOLVE_VOLATILE_EPOCH)
  bool drop_stale_(uint32_t expected);
#endif
#endif
  uint8_t state_() const {
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
    return this->state_word_.load();
#else
    return this->state_word_;
#endif
  }
  uint32_t addr_() const {
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
    return this->addr_word_.load();
#else
    return this->addr_word_;
#endif
  }
  void set_state_(uint8_t state) {
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
    this->state_word_.store(state);
#else
    this->state_word_ = state;
#endif
  }
  void set_addr_(uint32_t addr) {
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
    this->addr_word_.store(addr);
#else
    this->addr_word_ = addr;
#endif
  }
  const char *tag_{nullptr};
#if defined(IPV4_RESOLVE_ATOMIC_STATE)
  std::atomic<uint32_t> addr_word_{0};
  std::atomic<uint8_t> state_word_{STATE_IDLE};
#elif defined(IPV4_RESOLVE_VOLATILE_EPOCH)
  volatile uint32_t addr_word_{0};
  volatile uint32_t epoch_{0};
  volatile uint32_t pending_epoch_{0};
  volatile uint8_t state_word_{STATE_IDLE};
#else
  volatile uint32_t addr_word_{0};
  volatile uint8_t state_word_{STATE_IDLE};
#endif
};

}  // namespace esphome::socket

#endif
