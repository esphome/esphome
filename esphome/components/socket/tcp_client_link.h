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
/// lookup, the retry backoff and the fill level of the outgoing buffer;
/// TcpClientLink and TcpClientLinkSharedTx add where its bytes live. A fatal
/// read/write error closes the link and schedules the next attempt; the
/// caller sees the edge via connected().
class TcpClientLinkBase {
 public:
  static constexpr size_t TX_BUFFER_SIZE = 1024;

  void set_host(const char *host) { this->host_ = StringRef(host); }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }
  /// 0 leaves the link up. A full local buffer is not idle: the caller notes that
  /// with note_io(), because the peer may still be sending.
  void set_idle_timeout(uint32_t ms) { this->idle_timeout_ms_ = ms; }
  const char *host() const { return this->host_.c_str(); }
  uint16_t port() const { return this->port_; }
  uint32_t reconnect_interval() const { return this->reconnect_interval_ms_; }
  uint32_t idle_timeout() const { return this->idle_timeout_ms_; }

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
  void tx_commit(size_t len) { this->tx_len_ += static_cast<uint16_t>(len); }
  size_t tx_free() const { return this->connected_ ? TX_BUFFER_SIZE - this->tx_len_ : 0; }
  /// Close without scheduling a reconnect (shutdown).
  void close();
  /// Restart the idle clock. A consumer that cannot take more bytes calls this,
  /// so a full buffer is not treated as a quiet peer.
  void note_io() { this->last_io_ms_ = App.get_loop_component_start_time(); }
  /// When a byte last moved, or note_io() last ran.
  uint32_t last_io() const { return this->last_io_ms_; }
  /// Close when no byte has moved for idle_timeout(). Inline: the close runs only once that time has passed.
  void check_idle() {
    if (this->idle_timeout_ms_ != 0 && this->connected_ &&
        App.get_loop_component_start_time() - this->last_io_ms_ >= this->idle_timeout_ms_) {
      this->idle_close_();
    }
  }

  bool connected() const { return this->connected_; }
  /// A connect attempt is in flight.
  bool connecting() const { return this->sock_ != nullptr && !this->connected_; }
  bool ready() const { return this->sock_ != nullptr && this->sock_->ready(); }
  /// Shared retry clock, also usable for a listen socket.
  void note_attempt() { this->last_attempt_ms_ = App.get_loop_component_start_time(); }
  bool in_backoff() const {
    return App.get_loop_component_start_time() - this->last_attempt_ms_ < this->reconnect_interval_ms_;
  }

 protected:
  /// queue() and flush_tx() of the subclasses, on the buffer they keep.
  size_t queue_(uint8_t *tx, const uint8_t *data, size_t len);
  bool flush_tx_(uint8_t *tx) {
    if (this->tx_len_ != 0) {
      this->flush_tx_slow_(tx);
    }
    return this->tx_len_ == 0;
  }
  /// The raw stream write behind flush_tx(); drops the link on a fatal error.
  ssize_t write_(const uint8_t *buf, size_t len);
  void flush_tx_slow_(uint8_t *tx);
  void poll_slow_();
  void try_connect_();
  /// Close after a failure, log what and errno, schedule the next attempt.
  void drop_(const LogString *what, int err);
  /// Log and close after check_idle() saw the timeout pass; schedules the next attempt like a failure.
  void idle_close_();

  StringRef host_;
  std::unique_ptr<Socket> sock_;
  const char *tag_{nullptr};
  uint32_t last_attempt_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  uint32_t last_io_ms_{0};
  uint32_t idle_timeout_ms_{0};
  Ipv4Resolve resolved_;
  uint16_t port_{0};
  uint16_t tx_len_{0};
  bool connected_{false};
};

/// A link with its own outgoing buffer.
class TcpClientLink : public TcpClientLinkBase {
 public:
  /// Copy into the outgoing buffer; returns how many bytes fit.
  size_t queue(const uint8_t *data, size_t len) { return this->queue_(this->tx_, data, len); }
  /// Direct access to the buffer's free tail. Fill at most tx_free() bytes,
  /// then tx_commit() the count; neither is bounds checked.
  uint8_t *tx_tail() { return this->tx_ + this->tx_len_; }
  /// Send the front of the buffer; true once it is empty.
  /// A partial write keeps the rest; inline no-op while nothing is queued.
  bool flush_tx() { return this->flush_tx_(this->tx_); }

 protected:
  uint8_t tx_[TX_BUFFER_SIZE]{};
};

/// A link whose outgoing buffer belongs to its owner, so several links can share one.
/// Only one of them may hold bytes: the owner queues to another only after flush_tx() emptied the last.
class TcpClientLinkSharedTx : public TcpClientLinkBase {
 public:
  /// buf holds TX_BUFFER_SIZE bytes and outlives the link. Call before the link connects.
  void set_tx_buffer(uint8_t *buf) { this->tx_ = buf; }
  size_t queue(const uint8_t *data, size_t len) { return this->queue_(this->tx_, data, len); }
  bool flush_tx() { return this->flush_tx_(this->tx_); }

 protected:
  uint8_t *tx_{nullptr};
};

}  // namespace esphome::socket

#endif
