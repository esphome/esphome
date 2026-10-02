#pragma once

#include "headers.h"

#include <cstdint>
#include <cstring>

namespace esphome::socket {

/// One allowed IPv4 network, in network byte order with host bits cleared.
/// Codegen validates and emits these into flash; cv.ipv4network makes an
/// invalid or non contiguous mask unrepresentable.
struct Ipv4AllowEntry {
  uint32_t addr;
  uint32_t mask;
};

/// IPv4 peers that may connect. An empty list allows every peer.
class Ipv4Allow {
 public:
  void set(const Ipv4AllowEntry *entries, uint8_t count) {
    this->entries_ = entries;
    this->count_ = count;
  }

  /// True when the list is empty or the peer falls into one entry.
  /// A v4 mapped IPv6 peer is unwrapped; any other family fails a non empty list.
  bool allows(const struct sockaddr *peer) const {
    if (this->count_ == 0) {
      return true;
    }
    uint32_t addr;
    if (peer->sa_family == AF_INET) {
      addr = reinterpret_cast<const struct sockaddr_in *>(peer)->sin_addr.s_addr;
    }
#ifdef AF_INET6
    else if (peer->sa_family == AF_INET6) {
      static constexpr uint8_t V4_MAPPED_PREFIX[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
      const uint8_t *bytes = reinterpret_cast<const struct sockaddr_in6 *>(peer)->sin6_addr.s6_addr;
      if (memcmp(bytes, V4_MAPPED_PREFIX, sizeof(V4_MAPPED_PREFIX)) != 0) {
        return false;
      }
      memcpy(&addr, bytes + sizeof(V4_MAPPED_PREFIX), sizeof(addr));
    }
#endif
    else {
      return false;
    }
    return this->allows(addr);
  }

  /// addr is network byte order, as it sits in a sockaddr_in.
  bool allows(uint32_t addr) const {
    if (this->count_ == 0) {
      return true;
    }
    for (uint8_t i = 0; i != this->count_; i++) {
      if ((addr & this->entries_[i].mask) == this->entries_[i].addr) {
        return true;
      }
    }
    return false;
  }

 private:
  const Ipv4AllowEntry *entries_{nullptr};
  uint8_t count_{0};
};

}  // namespace esphome::socket
