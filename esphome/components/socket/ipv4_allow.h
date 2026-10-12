#pragma once

#include "headers.h"
#include "socket.h"
#include "esphome/core/hal.h"

#include <cstddef>
#include <cstdint>

namespace esphome::socket {

/// One allowed IPv4 network, network byte order, host bits cleared.
/// Lives in flash; read via progmem_memcpy.
struct Ipv4AllowEntry {
  uint32_t addr;
  uint32_t mask;
};

/// IPv4 peers that may connect. An empty list allows every peer.
class Ipv4Allow {
 public:
  void set(const Ipv4AllowEntry *entries, size_t count) {
    this->entries_ = entries;
    this->count_ = count;
  }

  /// A v4 mapped IPv6 peer is unwrapped; any other family fails a non empty list.
  bool allows(const struct sockaddr *peer) const {
    if (this->count_ == 0) {
      return true;
    }
    uint32_t addr;
    return sockaddr_to_ipv4(peer, &addr) && this->allows(addr);
  }

  /// addr is network byte order, as it sits in a sockaddr_in.
  bool allows(uint32_t addr) const {
    if (this->count_ == 0) {
      return true;
    }
    for (size_t i = 0; i != this->count_; i++) {
      Ipv4AllowEntry e = this->entry(i);
      if ((addr & e.mask) == e.addr) {
        return true;
      }
    }
    return false;
  }

  size_t size() const { return this->count_; }
  /// A copy of entry i, read from flash.
  Ipv4AllowEntry entry(size_t i) const {
    Ipv4AllowEntry e;
    progmem_memcpy(&e, &this->entries_[i], sizeof(e));
    return e;
  }

 private:
  const Ipv4AllowEntry *entries_{nullptr};
  size_t count_{0};
};

}  // namespace esphome::socket
