#include "tcp_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstring>

namespace esphome::tcp_uart {

static const char *const TAG = "tcp_uart";

void TcpUart::setup() {
  this->link_.begin(TAG);
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void TcpUart::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TCP UART:\n"
                "  Host: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms",
                this->link_.host(), this->link_.port(), this->link_.reconnect_interval());
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void TcpUart::sync_link_() {
  bool up = this->link_.connected();
  this->link_was_up_ = up;
  if (!up) {
    this->rx_.clear();
    this->rx_pending_ = false;
    this->tx_len_ = 0;
  }
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void TcpUart::read_socket_() {
  size_t room = RX_BUFFER_SIZE - this->rx_.size();
  if (room == 0) {
    this->rx_pending_ = true;
    return;
  }
  uint8_t tmp[READ_CHUNK];
  size_t want = room < sizeof(tmp) ? room : sizeof(tmp);
  ssize_t count = this->link_.read(tmp, want);
  if (count <= 0) {
    // A dropped link (-1) is cleaned up by sync_link_() on the next loop.
    if (count == 0) {
      this->rx_pending_ = false;
    }
    return;
  }
  for (ssize_t i = 0; i < count; i++) {
    this->rx_.push(tmp[i]);
  }
  this->rx_pending_ = static_cast<size_t>(count) == want;
}

void TcpUart::flush_tx_() {
  ssize_t sent = this->link_.write(this->tx_, this->tx_len_);
  if (sent > 0) {
    this->tx_len_ -= static_cast<uint16_t>(sent);
    std::memmove(this->tx_, this->tx_ + sent, this->tx_len_);
  }
}

void TcpUart::loop() {
  this->link_.poll();
  if (this->link_.connected() != this->link_was_up_) {
    this->sync_link_();
  }
  if (!this->link_was_up_) {
    return;
  }
  if (this->rx_pending_ || this->link_.ready()) {
    this->read_socket_();
  }
  if (this->tx_len_ != 0) {
    this->flush_tx_();
  }
}

void TcpUart::write_array(const uint8_t *data, size_t len) {
  size_t room = this->link_.connected() ? sizeof(this->tx_) - this->tx_len_ : 0;
  if (len > room) {
    uint32_t now = App.get_loop_component_start_time();
    if (this->last_drop_log_ms_ == 0 || now - this->last_drop_log_ms_ >= 5000) {
      ESP_LOGW(TAG, "%s, dropped %u bytes",
               this->link_.connected() ? LOG_STR_LITERAL("TX buffer full") : LOG_STR_LITERAL("Not connected"),
               static_cast<unsigned>(len - room));
      this->last_drop_log_ms_ = now;
    }
    len = room;
  }
  std::memcpy(this->tx_ + this->tx_len_, data, len);
  this->tx_len_ += static_cast<uint16_t>(len);
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
