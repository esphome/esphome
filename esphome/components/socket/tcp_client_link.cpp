#include "tcp_client_link.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cerrno>

namespace esphome::socket {

void set_stream_options(Socket *sock) {
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

void TcpClientLink::note_attempt() { this->last_attempt_ms_ = App.get_loop_component_start_time(); }

bool TcpClientLink::in_backoff() const {
  return App.get_loop_component_start_time() - this->last_attempt_ms_ < this->reconnect_interval_ms_;
}

void TcpClientLink::poll() {
  if (this->sock_ == nullptr) {
    if (!this->in_backoff()) {
      this->try_connect_();
    }
    return;
  }
  if (this->connected_) {
    return;
  }
  int err = 0;
  switch (poll_connect(*this->sock_, err)) {
    case ConnectPollResult::CONNECT_POLL_RESULT_PENDING:
      return;
    case ConnectPollResult::CONNECT_POLL_RESULT_ERROR:
      this->drop_(err);
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
    this->note_attempt();
    return;
  }
  set_stream_options(this->sock_.get());
  // An immediate success is reported by the next poll(); poll_connect() sees it writable.
  if (this->sock_->connect(reinterpret_cast<struct sockaddr *>(&dest), dest_len) != 0 && errno != EINPROGRESS) {
    this->drop_(errno);
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
    this->drop_(count == 0 ? 0 : errno);
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
  this->drop_(errno);
  return -1;
}

void TcpClientLink::close() {
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connected_ = false;
  this->resolved_.forget();
}

void TcpClientLink::drop_(int err) {
  ESP_LOGW(this->tag_, "Connection lost: %d", err);
  this->close();
  this->note_attempt();
}

}  // namespace esphome::socket

#endif
