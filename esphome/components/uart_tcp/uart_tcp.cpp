#include "uart_tcp.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace esphome::uart_tcp {

static const char *const TAG = "uart_tcp";

static void consume_buf(uint8_t *buf, size_t *len, size_t n) {
  if (n >= *len) {
    *len = 0;
    return;
  }
  std::memmove(buf, buf + n, *len - n);
  *len -= n;
}

void UartTcp::setup() {
  this->last_attempt_ms_ = App.get_loop_component_start_time() - this->reconnect_interval_ms_;
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void UartTcp::dump_config() {
  if (this->server_) {
    ESP_LOGCONFIG(TAG,
                  "UART TCP:\n"
                  "  Role: %s\n"
                  "  Listen: %u\n"
                  "  UART baud: %" PRIu32 "\n"
                  "  Reconnect Interval: %" PRIu32 "ms",
                  LOG_STR_LITERAL("server"), this->port_, this->parent_->get_baud_rate(), this->reconnect_interval_ms_);
  } else {
    ESP_LOGCONFIG(TAG,
                  "UART TCP:\n"
                  "  Role: %s\n"
                  "  Host: %s:%u\n"
                  "  UART baud: %" PRIu32 "\n"
                  "  Reconnect Interval: %" PRIu32 "ms",
                  LOG_STR_LITERAL("client"), this->host_.c_str(), this->port_, this->parent_->get_baud_rate(),
                  this->reconnect_interval_ms_);
  }
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void UartTcp::on_shutdown() {
  this->close_sock_();
  this->close_listen_();
}

void UartTcp::set_link_up_(bool up) {
  if (this->connected_ == up) {
    return;
  }
  this->connected_ = up;
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void UartTcp::close_sock_() {
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connecting_ = false;
  this->rx_pending_ = false;
  this->set_link_up_(false);
  this->tx_len_ = 0;
  this->resolved_.forget();
}

void UartTcp::close_listen_() { this->listen_.reset(); }

void UartTcp::apply_socket_options_(socket::Socket *sock) {
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

void UartTcp::try_connect_() {
  if (this->sock_ != nullptr || this->in_backoff_()) {
    return;
  }
  if (this->resolved_.consume_failure()) {
    this->note_attempt_();
    return;
  }
  this->resolved_.start(this->host_.c_str(), this->port_, TAG);
  if (!this->resolved_.ready()) {
    return;
  }
  struct sockaddr_storage dest;
  socklen_t dest_len =
      this->resolved_.to_sockaddr(reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest), this->port_);
  if (dest_len == 0) {
    this->note_attempt_();
    return;
  }
  this->sock_ = socket::socket_loop_monitored(dest.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (this->sock_ == nullptr) {
    this->note_attempt_();
    return;
  }
  this->apply_socket_options_(this->sock_.get());
  int rc = this->sock_->connect(reinterpret_cast<struct sockaddr *>(&dest), dest_len);
  if (rc == 0 || errno == EINPROGRESS) {
    this->connecting_ = rc != 0;
    if (rc == 0) {
      this->set_link_up_(true);
      ESP_LOGI(TAG, "Connected to %s:%u", this->host_.c_str(), this->port_);
    }
    return;
  }
  this->sock_.reset();
  this->resolved_.forget();
  this->note_attempt_();
}

void UartTcp::try_listen_() {
  if (this->listen_ != nullptr || this->in_backoff_()) {
    return;
  }
  this->listen_ = socket::socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_ == nullptr) {
    this->note_attempt_();
    return;
  }
  int yes = 1;
  this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  this->listen_->setblocking(false);
  struct sockaddr_storage local;
  socklen_t local_len =
      socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->port_);
  if (local_len == 0 || this->listen_->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) != 0 ||
      this->listen_->listen(1) != 0) {
    ESP_LOGW(TAG, "Listen on %u failed", this->port_);
    this->listen_.reset();
    this->note_attempt_();
    return;
  }
  ESP_LOGI(TAG, "Listening on %u", this->port_);
}

void UartTcp::accept_client_() {
  if (this->listen_ == nullptr || this->sock_ != nullptr) {
    return;
  }
  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof(peer);
  auto client = this->listen_->accept_loop_monitored(reinterpret_cast<struct sockaddr *>(&peer), &peer_len);
  if (client == nullptr) {
    return;
  }
  this->apply_socket_options_(client.get());
  this->sock_ = std::move(client);
  this->set_link_up_(true);
  ESP_LOGI(TAG, "Client connected");
}

void UartTcp::read_socket_() {
  if (this->sock_ == nullptr) {
    return;
  }
  if (this->connecting_) {
    int err = 0;
    switch (socket::poll_connect(*this->sock_, err)) {
      case socket::ConnectPollResult::CONNECT_POLL_RESULT_PENDING:
        return;
      case socket::ConnectPollResult::CONNECT_POLL_RESULT_ERROR:
        ESP_LOGW(TAG, "Connection failed: %d", err);
        this->close_sock_();
        this->note_attempt_();
        return;
      default:
        break;
    }
    this->connecting_ = false;
    this->set_link_up_(true);
    ESP_LOGI(TAG, "Connected to %s:%u", this->host_.c_str(), this->port_);
  }
  uint8_t tmp[READ_CHUNK];
  ssize_t count = this->sock_->read(tmp, sizeof(tmp));
  if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    ESP_LOGW(TAG, "Connection lost");
    this->close_sock_();
    this->note_attempt_();
    return;
  }
  if (count < 0) {
    this->rx_pending_ = false;
    return;
  }
  this->rx_pending_ = static_cast<size_t>(count) == sizeof(tmp);
  this->write_array(tmp, static_cast<size_t>(count));
}

void UartTcp::flush_tx_() {
  if (!this->connected_ || this->sock_ == nullptr || this->tx_len_ == 0) {
    return;
  }
  ssize_t sent = this->sock_->write(this->tx_, this->tx_len_);
  if (sent > 0) {
    consume_buf(this->tx_, &this->tx_len_, static_cast<size_t>(sent));
    return;
  }
  if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    ESP_LOGW(TAG, "Send failed");
    this->close_sock_();
    this->note_attempt_();
  }
}

void UartTcp::read_uart_() {
  size_t room = TX_BUFFER_SIZE - this->tx_len_;
  if (room == 0) {
    return;
  }
  uint8_t tmp[READ_CHUNK];
  size_t want = this->available();
  if (want > sizeof(tmp)) {
    want = sizeof(tmp);
  }
  if (want > room) {
    want = room;
  }
  if (want == 0 || !this->read_array(tmp, want)) {
    return;
  }
  std::memcpy(this->tx_ + this->tx_len_, tmp, want);
  this->tx_len_ += want;
}

void UartTcp::loop() {
  if (this->server_) {
    if (this->listen_ == nullptr && !this->in_backoff_()) {
      this->try_listen_();
    }
    if (this->sock_ == nullptr && this->listen_ != nullptr && this->listen_->ready()) {
      this->accept_client_();
    }
  } else if (this->sock_ == nullptr && !this->in_backoff_()) {
    this->try_connect_();
  }
  if (this->sock_ != nullptr && (this->connecting_ || this->rx_pending_ || this->sock_->ready())) {
    this->read_socket_();
  }
  if (this->tx_len_ != 0) {
    this->flush_tx_();
  }
  if (this->connected_ && this->available() > 0) {
    this->read_uart_();
  }
}

}  // namespace esphome::uart_tcp
