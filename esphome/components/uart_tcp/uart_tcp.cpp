#include "uart_tcp.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::uart_tcp {

static const char *const TAG = "uart_tcp";

void UartTcp::setup() {
  this->link_.begin(TAG);
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void UartTcp::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART TCP:\n"
                "  %s: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms",
                this->server_ ? LOG_STR_LITERAL("Listen") : LOG_STR_LITERAL("Host"),
                this->server_ ? LOG_STR_LITERAL("*") : this->link_.host(), this->link_.port(),
                this->link_.reconnect_interval());
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void UartTcp::on_shutdown() {
  this->link_.close();
  this->listen_.reset();
}

void UartTcp::sync_link_() {
  bool up = this->link_.connected();
  this->link_was_up_ = up;
  if (!up) {
    this->tx_len_ = 0;
    this->rx_pending_ = false;
  }
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void UartTcp::try_listen_() {
  this->listen_ = socket::socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_ == nullptr) {
    this->link_.note_attempt();
    return;
  }
  int yes = 1;
  this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  this->listen_->setblocking(false);
  struct sockaddr_storage local;
  socklen_t local_len =
      socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->link_.port());
  if (local_len == 0 || this->listen_->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) != 0 ||
      this->listen_->listen(1) != 0) {
    ESP_LOGW(TAG, "Listen on %u failed", this->link_.port());
    this->listen_.reset();
    this->link_.note_attempt();
    return;
  }
  ESP_LOGI(TAG, "Listening on %u", this->link_.port());
}

void UartTcp::accept_client_() {
  auto client = this->listen_->accept_loop_monitored(nullptr, nullptr);
  if (client == nullptr) {
    return;
  }
  this->link_.adopt(std::move(client));
  ESP_LOGI(TAG, "Client connected");
}

void UartTcp::read_socket_() {
  uint8_t tmp[READ_CHUNK];
  ssize_t count = this->link_.read(tmp, sizeof(tmp));
  if (count <= 0) {
    // A dropped link (-1) is cleaned up by sync_link_() on the next loop.
    if (count == 0) {
      this->rx_pending_ = false;
    }
    return;
  }
  this->rx_pending_ = static_cast<size_t>(count) == sizeof(tmp);
  this->write_array(tmp, static_cast<size_t>(count));
}

void UartTcp::flush_tx_() {
  ssize_t sent = this->link_.write(this->tx_, this->tx_len_);
  if (sent > 0) {
    this->tx_len_ -= static_cast<uint16_t>(sent);
    std::memmove(this->tx_, this->tx_ + sent, this->tx_len_);
  }
}

void UartTcp::read_uart_() {
  size_t want = std::min<size_t>(this->available(), TX_BUFFER_SIZE - this->tx_len_);
  if (want != 0 && this->read_array(this->tx_ + this->tx_len_, want)) {
    this->tx_len_ += static_cast<uint16_t>(want);
  }
}

void UartTcp::loop() {
  if (this->server_) {
    if (this->listen_ == nullptr && !this->link_.in_backoff()) {
      this->try_listen_();
    }
    if (this->listen_ != nullptr && !this->link_.connected() && this->listen_->ready()) {
      this->accept_client_();
    }
  } else {
    this->link_.poll();
  }
  if (this->link_.connected() != this->link_was_up_) {
    this->sync_link_();
  }
  if (!this->link_was_up_) {
    return;
  }
  if (this->rx_pending_ || this->link_.ready()) {
    this->read_socket_();
  }
  // UART bytes picked up here go out in the same pass.
  this->read_uart_();
  if (this->tx_len_ != 0) {
    this->flush_tx_();
  }
}

}  // namespace esphome::uart_tcp
