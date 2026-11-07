#pragma once

#include "headers.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#ifdef USE_SOCKET_IPV4_ALLOW
#include "ipv4_allow.h"
#endif
#include "socket.h"
#include "tcp_client_link.h"

#include <cstdint>
#include <memory>

namespace esphome::socket {

/// The server side of a bridged TCP link: owns the listen socket and the
/// allow list, accepts one peer at a time and adopts it into a TcpClientLink,
/// sharing that link's retry clock and connect port.
class TcpListener {
 public:
#ifdef USE_SOCKET_IPV4_ALLOW
  void set_allow(const Ipv4AllowEntry *entries, size_t count) { this->allow_.set(entries, count); }
#endif

  /// Call from setup(); tag names the log lines.
  void begin(const char *tag) { this->tag_ = tag; }
  /// Server state machine; call every loop. may_accept lets the caller hold
  /// accepts until its own disconnect edge has run.
  void poll(TcpClientLink &link, bool may_accept) {
    if (this->listen_ == nullptr) {
      if (!link.in_backoff()) {
        this->try_listen_(link);
      }
      return;
    }
    if (may_accept && !link.connected() && this->listen_->ready()) {
      this->accept_(link);
    }
  }
  void close() { this->listen_.reset(); }
  /// One config line per allowed network.
  void dump_config() const;

 protected:
  void try_listen_(TcpClientLink &link);
  void accept_(TcpClientLink &link);

  std::unique_ptr<ListenSocket> listen_;
  const char *tag_{nullptr};
#ifdef USE_SOCKET_IPV4_ALLOW
  uint32_t last_reject_log_ms_{0};
  Ipv4Allow allow_;
#endif
};

}  // namespace esphome::socket

#endif
