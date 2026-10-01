#pragma once

#include "headers.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "ipv4_resolve.h"
#include "socket.h"
#include "esphome/core/string_ref.h"

#include <cstdint>
#include <memory>

namespace esphome::socket {

/// Non-blocking options and TCP keepalive for a bridged stream socket.
void set_stream_options(Socket *sock);

/// A reconnecting TCP stream driven from loop(). Owns the socket, the DNS
/// lookup and the retry backoff. A fatal read/write error closes the link
/// and schedules the next attempt; the caller sees the edge via connected().
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
  void poll();
  /// Take over an accepted socket (the server side of a bridge).
  void adopt(std::unique_ptr<Socket> sock);
  /// Returns bytes moved, 0 when nothing can move now, -1 when the link dropped.
  ssize_t read(uint8_t *buf, size_t len);
  ssize_t write(const uint8_t *buf, size_t len);
  /// Close without scheduling a reconnect (shutdown).
  void close();

  bool connected() const { return this->connected_; }
  bool ready() const { return this->sock_ != nullptr && this->sock_->ready(); }
  /// Shared retry clock, also usable for a listen socket.
  void note_attempt();
  bool in_backoff() const;

 protected:
  void try_connect_();
  /// Close after a failure, log it and schedule the next attempt.
  void drop_(int err);

  StringRef host_;
  std::unique_ptr<Socket> sock_;
  const char *tag_{nullptr};
  uint32_t last_attempt_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  Ipv4Resolve resolved_;
  uint16_t port_{0};
  bool connected_{false};
};

}  // namespace esphome::socket

#endif
