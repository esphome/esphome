#include "tcp_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace esphome::tcp_uart {

static const char *const TAG = "tcp_uart";

static void consume_buf(uint8_t *buf, size_t *len, size_t n) {
  if (n >= *len) {
    *len = 0;
    return;
  }
  std::memmove(buf, buf + n, *len - n);
  *len -= n;
}

void TcpUart::setup() {
  // The first attempt must not wait out a full interval.
  this->last_attempt_ms_ = App.get_loop_component_start_time() - this->reconnect_interval_ms_;
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void TcpUart::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TCP UART:\n"
                "  Host: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms",
                this->host_.c_str(), this->port_, this->reconnect_interval_ms_);
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void TcpUart::on_shutdown() { this->close_sock_(); }

void TcpUart::set_link_up_(bool up) {
  if (this->connected_ == up) {
    return;
  }
  this->connected_ = up;
  if (!up) {
    this->offline_drop_logged_ = false;
  }
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void TcpUart::close_sock_() {
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connecting_ = false;
  this->rx_pending_ = false;
  this->set_link_up_(false);
  this->rx_.clear();
  this->tx_len_ = 0;
  this->resolved_.forget();
}

void TcpUart::apply_socket_options_(socket::Socket *sock) {
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

void TcpUart::try_connect_() {
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

void TcpUart::read_socket_() {
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
  size_t room = RX_BUFFER_SIZE - this->rx_.size();
  if (room == 0) {
    this->rx_pending_ = true;
    return;
  }
  uint8_t tmp[READ_CHUNK];
  size_t want = room < sizeof(tmp) ? room : sizeof(tmp);
  ssize_t count = this->sock_->read(tmp, want);
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
  for (ssize_t i = 0; i < count; i++) {
    this->rx_.push(tmp[i]);
  }
  this->rx_pending_ = static_cast<size_t>(count) == want;
}

void TcpUart::flush_tx_() {
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

void TcpUart::loop() {
  if (this->sock_ == nullptr) {
    if (!this->in_backoff_()) {
      this->try_connect_();
    }
    return;
  }
  if (this->connecting_ || this->rx_pending_ || this->sock_->ready()) {
    if (this->connecting_ || this->rx_.size() < RX_BUFFER_SIZE) {
      this->read_socket_();
    }
  }
  if (this->tx_len_ != 0) {
    this->flush_tx_();
  }
}

void TcpUart::write_array(const uint8_t *data, size_t len) {
  if (!this->connected_) {
    if (len > 0 && !this->offline_drop_logged_) {
      ESP_LOGW(TAG, "Not connected, dropped %u bytes", static_cast<unsigned>(len));
      this->offline_drop_logged_ = true;
    }
    return;
  }
  size_t room = sizeof(this->tx_) - this->tx_len_;
  if (len > room) {
    uint32_t now = App.get_loop_component_start_time();
    if (this->last_drop_log_ms_ == 0 || now - this->last_drop_log_ms_ >= 5000) {
      ESP_LOGW(TAG, "TX buffer full, dropped %u bytes", static_cast<unsigned>(len - room));
      this->last_drop_log_ms_ = now;
    }
    len = room;
  }
  std::memcpy(this->tx_ + this->tx_len_, data, len);
  this->tx_len_ += len;
}

bool TcpUart::peek_byte(uint8_t *data) {
  if (this->rx_.empty()) {
    return false;
  }
  *data = this->rx_.front();
  return true;
}

bool TcpUart::read_array(uint8_t *data, size_t len) {
  if (this->rx_.size() < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = this->rx_.front();
    this->rx_.pop();
  }
  return true;
}

uart::UARTFlushResult TcpUart::flush() {
  this->flush_tx_();
  if (this->tx_len_ == 0) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
  }
  return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
}

}  // namespace esphome::tcp_uart
