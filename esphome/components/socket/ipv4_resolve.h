#pragma once

#include "headers.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include <atomic>
#include <cstdint>

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#include "lwip/ip_addr.h"
#endif

namespace esphome::socket {

/// One IPv4 literal or hostname. The address is stored as an integer. Flags are load and store only.
class Ipv4Resolve {
 public:
  void forget();
  /// Drop a failed lookup so the next start() tries again.
  bool consume_failure() {
    if (this->failed_.load() == 0) {
      return false;
    }
    this->failed_.store(0);
    this->have_.store(false);
    return true;
  }
  bool ready() const { return this->have_.load(); }
  /// Write the stored address into dest. Returns 0 until ready() is true.
  socklen_t to_sockaddr(struct sockaddr *dest, socklen_t destlen, uint16_t port) const;
  /// Resolve host. tag is used for the failure log, including the async callback.
  void start(const char *host, uint16_t port, const char *tag);

 private:
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  static void dns_found(const char *name, const ip_addr_t *addr, void *arg);
#endif
  const char *tag_{nullptr};
  std::atomic<uint32_t> addr_{0};
  std::atomic<uint32_t> epoch_{0};
  std::atomic<uint32_t> pending_epoch_{0};
  std::atomic<uint8_t> resolving_{0};
  std::atomic<uint8_t> failed_{0};
  std::atomic<uint8_t> have_{0};
};

}  // namespace esphome::socket

#endif
