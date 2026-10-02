#pragma once

#include "headers.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "ipv4_resolve.h"
#include "socket.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

#include <cstdint>
#include <memory>

namespace esphome::socket {

/// A reconnecting TCP stream driven from loop(). Owns the socket, the DNS
/// lookup, the retry backoff and the outgoing buffer. A fatal read/write
/// error closes the link and schedules the next attempt; the caller sees
/// the edge via connected().
class TcpClientLink {
 public:
  void set_host(const char *host) { this->host_ = StringRef(host); }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }
  const char *host() const { return this->host_.c_str(); }
  uint16_t port() const { return this->port_; }
  uint32_t reconnect_interval() const { return this->reconnect_interval_ms_; }

  /// Call from setup(). tag names this link's log lines.
  void begin(const char *tag);
  /// Connect state machine; call every loop while acting as a client.
  /// Inline no-op while connected or waiting out the backoff.
  void poll() {
    if (this->connected_ || (this->sock_ == nullptr && this->in_backoff())) {
      return;
    }
    this->poll_slow_();
  }
  /// Take over an accepted socket (the server side of a bridge).
  void adopt(std::unique_ptr<Socket> sock);
  /// Returns bytes moved, 0 when nothing can move now, -1 when the link dropped.
  ssize_t read(uint8_t *buf, size_t len);
  /// Copy into the outgoing buffer; returns how many bytes fit.
  size_t queue(const uint8_t *data, size_t len);
  /// Direct access to the buffer's free tail; tx_commit() what was filled.
  uint8_t *tx_tail() { return this->tx_ + this->tx_len_; }
  void tx_commit(size_t len) { this->tx_len_ += static_cast<uint16_t>(len); }
  size_t tx_free() const { return this->connected_ ? TX_BUFFER_SIZE - this->tx_len_ : 0; }
  /// Send the front of the buffer; true once it is empty.
  /// A partial write keeps the rest; inline no-op while nothing is queued.
  bool flush_tx() {
    if (this->tx_len_ != 0) {
      this->flush_tx_slow_();
    }
    return this->tx_len_ == 0;
  }
  /// Close without scheduling a reconnect (shutdown).
  void close();

  bool connected() const { return this->connected_; }
  bool ready() const { return this->sock_ != nullptr && this->sock_->ready(); }
  /// Shared retry clock, also usable for a listen socket.
  void note_attempt() { this->last_attempt_ms_ = App.get_loop_component_start_time(); }
  bool in_backoff() const {
    return App.get_loop_component_start_time() - this->last_attempt_ms_ < this->reconnect_interval_ms_;
  }

 protected:
  static constexpr size_t TX_BUFFER_SIZE = 1024;

  /// The raw stream write behind flush_tx(); drops the link on a fatal error.
  ssize_t write_(const uint8_t *buf, size_t len);
  void flush_tx_slow_();
  void poll_slow_();
  void try_connect_();
  /// Close after a failure, log what and errno, schedule the next attempt.
  void drop_(const LogString *what, int err);

  StringRef host_;
  std::unique_ptr<Socket> sock_;
  const char *tag_{nullptr};
  uint32_t last_attempt_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  Ipv4Resolve resolved_;
  uint16_t port_{0};
  uint16_t tx_len_{0};
  bool connected_{false};
  uint8_t tx_[TX_BUFFER_SIZE]{};
};

}  // namespace esphome::socket

#endif
