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
  /// Connections the stack may queue for accept(); call before the first poll.
  void set_backlog(uint8_t backlog) { this->backlog_ = backlog; }

  /// Call from setup(); tag names the log lines.
  void begin(const char *tag) { this->tag_ = tag; }
  /// Server state machine; call every loop. may_accept lets the caller hold
  /// accepts until its own disconnect edge has run.
  void poll(TcpClientLink &link, bool may_accept) {
    if (this->poll_listen(link) && may_accept && !link.connected()) {
      this->accept_into_(link);
    }
  }
  /// Opens the listen socket on clock's port and retry clock; true when a connection waits.
  bool poll_listen(TcpClientLink &clock) {
    if (this->listen_ == nullptr) {
      if (!clock.in_backoff()) {
        this->try_listen_(clock);
      }
      return false;
    }
    return this->listen_->ready();
  }
  /// Accepts a waiting connection that the allow list admits, or returns nullptr.
  /// Running out of sockets or memory keeps the listen socket for the next call and logs only the first failure
  /// until an accept succeeds; any other listener error closes it until clock's backoff has passed.
  std::unique_ptr<Socket> accept(TcpClientLink &clock);
  void close() { this->listen_.reset(); }
  /// One config line per allowed network.
  void dump_config() const;

 protected:
  void try_listen_(TcpClientLink &link);
  void accept_into_(TcpClientLink &link);

  std::unique_ptr<ListenSocket> listen_;
  const char *tag_{nullptr};
#ifdef USE_SOCKET_IPV4_ALLOW
  Ipv4Allow allow_;
#endif
  // One client at a time by default; a second connection waits in the stack until the first drops.
  uint8_t backlog_{1};
  // Set while a run of out-of-sockets accept failures has been logged.
  bool accept_failed_logged_{false};
};

}  // namespace esphome::socket

#endif
