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
/// allow list. The link supplies the listen port and the retry backoff;
/// accept() hands each admitted socket to the caller.
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
    if (this->poll_listen(link) && may_accept && !link.connected()) {
      if (auto client = this->accept(link)) {
        link.adopt(std::move(client));
      }
    }
  }
  /// Opens the listen socket on link's port after its backoff; true when a connection waits.
  /// A backlog of 1 serves one client at a time; a second waits in the stack until the first drops.
  bool poll_listen(TcpClientLink &link, uint8_t backlog = 1) {
    if (this->listen_ == nullptr) {
      if (!link.in_backoff()) {
        this->try_listen_(link, backlog);
      }
      return false;
    }
    return this->listen_->ready();
  }
  /// Accepts a waiting connection that the allow list admits, or returns nullptr.
  /// A listener error closes the listen socket until link's backoff has passed.
  std::unique_ptr<Socket> accept(TcpClientLink &link);
  void close() { this->listen_.reset(); }
  /// One config line per allowed network.
  void dump_config() const;

 protected:
  void try_listen_(TcpClientLink &link, uint8_t backlog);

  std::unique_ptr<ListenSocket> listen_;
  const char *tag_{nullptr};
#ifdef USE_SOCKET_IPV4_ALLOW
  Ipv4Allow allow_;
#endif
};

}  // namespace esphome::socket

#endif
