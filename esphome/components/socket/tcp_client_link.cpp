#include "tcp_client_link.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace esphome::socket {

// After this long in SYN, the stack's own retries are cut short.
static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;

// Non-blocking options and TCP keepalive for a bridged stream socket.
// Keepalive is best-effort: the raw lwIP implementation (ESP8266, RP2040)
// rejects it, so a half-open link there is only detected by a failed write.
static void set_stream_options(Socket *sock) {
  int yes = 1;
  sock->setblocking(false);
  sock->setsockopt(IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
  sock->setsockopt(SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
  int idle = 30;
  int interval = 10;
  int count = 3;
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
}

void TcpClientLink::begin(const char *tag) {
  this->tag_ = tag;
  // The first attempt must not wait out a full interval.
  this->last_attempt_ms_ = App.get_loop_component_start_time() - this->reconnect_interval_ms_;
}

void TcpClientLink::poll_slow_() {
  if (this->sock_ == nullptr) {
    this->try_connect_();
    return;
  }
  int err = 0;
  switch (poll_connect(*this->sock_, err)) {
    case ConnectPollResult::CONNECT_POLL_RESULT_PENDING:
      // Give up before the stack's SYN retries do, so the interval stays honest
      // and the next attempt resolves the host again.
      if (App.get_loop_component_start_time() - this->last_attempt_ms_ >=
          std::max(this->reconnect_interval_ms_, CONNECT_TIMEOUT_MS)) {
        this->drop_(LOG_STR("Connect failed"), ETIMEDOUT);
      }
      return;
    case ConnectPollResult::CONNECT_POLL_RESULT_ERROR:
      this->drop_(LOG_STR("Connect failed"), err);
      return;
    default:
      break;
  }
  this->connected_ = true;
  ESP_LOGI(this->tag_, "Connected to %s:%u", this->host_.c_str(), this->port_);
}

void TcpClientLink::try_connect_() {
  if (this->resolved_.consume_failure()) {
    this->note_attempt();
    return;
  }
  this->resolved_.start(this->host_.c_str(), this->port_, this->tag_);
  if (!this->resolved_.ready()) {
    return;
  }
  struct sockaddr_storage dest;
  socklen_t dest_len =
      this->resolved_.to_sockaddr(reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest), this->port_);
  if (dest_len == 0) {
    this->note_attempt();
    return;
  }
  this->sock_ = socket_loop_monitored(dest.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (this->sock_ == nullptr) {
    this->drop_(LOG_STR("Connect failed"), errno);
    return;
  }
  set_stream_options(this->sock_.get());
  // Starts the pending-connect clock that poll() times out against.
  this->note_attempt();
  // An immediate success is reported by the next poll(); poll_connect() sees it writable.
  if (this->sock_->connect(reinterpret_cast<struct sockaddr *>(&dest), dest_len) != 0 && errno != EINPROGRESS) {
    this->drop_(LOG_STR("Connect failed"), errno);
  }
}

void TcpClientLink::adopt(std::unique_ptr<Socket> sock) {
  this->close();
  set_stream_options(sock.get());
  this->sock_ = std::move(sock);
  this->connected_ = true;
}

ssize_t TcpClientLink::read(uint8_t *buf, size_t len) {
  if (!this->connected_) {
    return 0;
  }
  ssize_t count = this->sock_->read(buf, len);
  if (count > 0) {
    return count;
  }
  if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
    this->drop_(LOG_STR("Connection lost"), count == 0 ? 0 : errno);
    return -1;
  }
  return 0;
}

ssize_t TcpClientLink::write(const uint8_t *buf, size_t len) {
  if (!this->connected_ || len == 0) {
    return 0;
  }
  ssize_t sent = this->sock_->write(buf, len);
  if (sent >= 0) {
    return sent;
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return 0;
  }
  this->drop_(LOG_STR("Connection lost"), errno);
  return -1;
}

size_t TcpClientLink::queue(const uint8_t *data, size_t len) {
  size_t room = this->tx_free();
  if (len > room) {
    len = room;
  }
  std::memcpy(this->tx_ + this->tx_len_, data, len);
  this->tx_len_ += static_cast<uint16_t>(len);
  return len;
}

void TcpClientLink::flush_tx() {
  ssize_t sent = this->write(this->tx_, this->tx_len_);
  if (sent > 0) {
    this->tx_len_ -= static_cast<uint16_t>(sent);
    std::memmove(this->tx_, this->tx_ + sent, this->tx_len_);
  }
}

void TcpClientLink::close() {
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connected_ = false;
  this->tx_len_ = 0;
  this->resolved_.forget();
}

void TcpClientLink::drop_(const LogString *what, int err) {
  ESP_LOGW(this->tag_, "%s: %d", LOG_STR_ARG(what), err);
  this->close();
  this->note_attempt();
}

}  // namespace esphome::socket

#endif
