#include "tcp_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstring>

namespace esphome::tcp_uart {

static const char *const TAG = "tcp_uart";

static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;

void TcpUart::setup() {
  this->link_.begin(TAG);
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.begin(TAG);
#endif
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void TcpUart::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TCP UART:\n"
                "  %s: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms",
                this->server_ ? LOG_STR_LITERAL("Listen") : LOG_STR_LITERAL("Host"),
                this->server_ ? LOG_STR_LITERAL("*") : this->link_.host(), this->link_.port(),
                this->link_.reconnect_interval());
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.dump_config();
#endif
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void TcpUart::on_shutdown() {
  this->link_.close();
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.close();
#endif
}

void TcpUart::sync_link_() {
  bool up = this->link_.connected();
  this->link_was_up_ = up;
  if (up) {
    // Unread bytes of the last session stay readable while down, never into the next one.
    this->rx_start_ = this->rx_end_ = 0;
  }
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void TcpUart::read_socket_() {
  if (this->rx_start_ != 0) {
    this->rx_end_ -= this->rx_start_;
    std::memmove(this->rx_, this->rx_ + this->rx_start_, this->rx_end_);
    this->rx_start_ = 0;
  }
  size_t room = RX_BUFFER_SIZE - this->rx_end_;
  if (room == 0) {
    // Only a read that filled all free space gets here, so rx_pending_ is already set.
    return;
  }
  ssize_t count = this->link_.read(this->rx_ + this->rx_end_, room);
  if (count <= 0) {
    // A dropped link (-1) is seen by sync_link_() on the next loop.
    if (count == 0) {
      this->rx_pending_ = false;
    }
    return;
  }
  this->rx_end_ += static_cast<uint16_t>(count);
  this->rx_pending_ = static_cast<size_t>(count) == room;
}

void TcpUart::loop() {
#ifdef USE_SOCKET_TCP_LISTENER
  if (this->server_) {
    // link_was_up_ holds the accept until the previous drop's edge has run,
    // so the sensor sees the disconnect and the new session's edge clears RX.
    this->listener_.poll(this->link_, !this->link_was_up_);
  } else {
    this->link_.poll();
  }
#else
  this->link_.poll();
#endif
  if (this->link_.connected() != this->link_was_up_) {
    this->sync_link_();
  }
  if (!this->link_was_up_) {
    return;
  }
  if (this->rx_pending_ || this->link_.ready()) {
    this->read_socket_();
  }
  this->link_.flush_tx();
}

void TcpUart::write_array(const uint8_t *data, size_t len) {
  size_t queued = this->link_.queue(data, len);
  if (queued < len) {
    uint32_t now = App.get_loop_component_start_time();
    if (this->last_drop_log_ms_ == 0 || now - this->last_drop_log_ms_ >= DROP_LOG_INTERVAL_MS) {
      ESP_LOGW(TAG, "%s, dropped %u bytes",
               this->link_.connected() ? LOG_STR_LITERAL("TX buffer full") : LOG_STR_LITERAL("Not connected"),
               static_cast<unsigned>(len - queued));
      this->last_drop_log_ms_ = now;
    }
  }
}

bool TcpUart::peek_byte(uint8_t *data) {
  if (this->rx_start_ == this->rx_end_) {
    return false;
  }
  *data = this->rx_[this->rx_start_];
  return true;
}

bool TcpUart::read_array(uint8_t *data, size_t len) {
  if (this->available() < len) {
    return false;
  }
  std::memcpy(data, this->rx_ + this->rx_start_, len);
  this->rx_start_ += static_cast<uint16_t>(len);
  return true;
}

uart::UARTFlushResult TcpUart::flush() {
  bool emptied = this->link_.flush_tx();
  if (!this->link_.connected()) {
    // A down link cannot have delivered anything, whether this flush dropped
    // it or an earlier loop() write did.
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  return emptied ? uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS : uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
}

}  // namespace esphome::tcp_uart
