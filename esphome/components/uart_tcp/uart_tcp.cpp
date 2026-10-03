#include "uart_tcp.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cerrno>
#include <cinttypes>

namespace esphome::uart_tcp {

static const char *const TAG = "uart_tcp";

// Bytes per 16 ms loop pass at 10 bits per byte: baud / 10 / 62.5.
static constexpr uint32_t BAUD_PACE_DIVISOR = 625;

void UartTcp::setup() {
  this->link_.begin(TAG);
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.begin(TAG);
#endif
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
  ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 "ms", this->timeout_ms_);
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.dump_config();
#endif
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void UartTcp::on_shutdown() {
  this->link_.close();
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.close();
#endif
}

void UartTcp::sync_link_() {
  bool up = this->link_.connected();
  this->link_was_up_ = up;
  if (up) {
    // The driver kept whatever arrived while the link was down.
    this->discard_uart_();
    // The limit is measured from the moment the link came up, not from boot.
    this->note_io_();
  } else {
    this->last_io_ms_ = 0;
  }
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void UartTcp::read_socket_() {
  // A hardware write blocks until the driver takes every byte. Leave what does
  // not fit in the socket, so TCP flow control throttles the peer.
  size_t room = this->parent_->available_for_write();
  if (room == SIZE_MAX) {
    // Capacity unknown on this platform; pace to one loop pass of UART time
    // (16 ms at 10 bits per byte) so a blocking write stays short.
    room = std::max<size_t>(1, this->parent_->get_baud_rate() / BAUD_PACE_DIVISOR);
  }
  if (room == 0) {
    this->rx_pending_ = true;
    return;
  }
  uint8_t tmp[READ_CHUNK];
  size_t want = std::min(room, sizeof(tmp));
  ssize_t count = this->link_.read(tmp, want);
  if (count <= 0) {
    // A dropped link (-1) is cleaned up by sync_link_() on the next loop.
    if (count == 0) {
      this->rx_pending_ = false;
    }
    return;
  }
  this->rx_pending_ = static_cast<size_t>(count) == want;
  this->write_array(tmp, static_cast<size_t>(count));
  this->note_io_();
}

void UartTcp::discard_uart_() {
  // Drain exactly what was buffered while the link was down; later bytes are live.
  uint8_t dump[32];
  size_t left = this->available();
  while (left != 0) {
    size_t n = std::min(left, sizeof(dump));
    if (!this->read_array(dump, n)) {
      return;
    }
    left -= n;
  }
}

void UartTcp::read_uart_() {
  size_t want = std::min<size_t>(this->available(), this->link_.tx_free());
  if (want != 0 && this->read_array(this->link_.tx_tail(), want)) {
    this->link_.tx_commit(want);
  }
}

void UartTcp::loop() {
#ifdef USE_SOCKET_TCP_LISTENER
  if (this->server_) {
    // link_was_up_ holds the accept until the previous drop's edge has run,
    // so the sensor and the stale UART discard always see the disconnect.
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
  this->check_idle_();
  if (!this->link_.connected()) {
    return;
  }
  if (this->rx_pending_ || this->link_.ready()) {
    this->read_socket_();
  }
  // UART bytes picked up here go out in the same pass.
  this->read_uart_();
  size_t before = this->link_.tx_free();
  this->link_.flush_tx();
  if (this->link_.tx_free() > before) {
    this->note_io_();
  }
}

void UartTcp::close_idle_() {
  ESP_LOGW(TAG, "Link idle, closing");
  this->link_.close();
  this->link_.note_attempt();
}

}  // namespace esphome::uart_tcp
