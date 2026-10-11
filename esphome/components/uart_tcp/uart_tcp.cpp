#include "uart_tcp.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cerrno>
#include <cinttypes>

namespace esphome::uart_tcp {

ESPHOME_LOG_TAG(TAG, "uart_tcp");

void UartTcp::setup() {
  this->link_.begin(TAG);
#ifdef USE_NOISE_STREAM
  if (this->noise_ != nullptr) {
    this->noise_->set_log_tag(TAG);
  }
#endif
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.begin(TAG);
#endif
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
#ifdef USE_SENSOR
  if (this->disconnects_sensor_ != nullptr) {
    this->disconnects_sensor_->publish_state(0);
  }
#endif
}

void UartTcp::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART TCP:\n"
                "  %s: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms",
                this->server_ ? LOG_STR_LITERAL("Listen") : LOG_STR_LITERAL("Host"),
                this->server_ ? LOG_STR_LITERAL("*") : this->link_.host(), this->link_.port(),
                this->link_.reconnect_interval());
  if (this->link_.idle_timeout() != 0) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 "ms", this->link_.idle_timeout());
  }
#ifdef USE_NOISE_STREAM
  if (this->noise_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Encryption: Noise");
  }
#endif
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.dump_config();
#endif
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Disconnects", this->disconnects_sensor_);
#endif
}

void UartTcp::on_shutdown() {
  this->link_.close();
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.close();
#endif
}

void UartTcp::sync_link_(bool up) {
  this->link_was_up_ = up;
  if (up) {
    // The driver kept whatever arrived while the link was down.
    this->discard_uart_();
#ifdef USE_NOISE_STREAM
    // The handshake's last read can leave session bytes the socket's ready flag does not show
    if (this->noise_ != nullptr) {
      this->rx_pending_ = true;
    }
#endif
  }
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
#ifdef USE_SENSOR
  // Only edges get here, so down is the falling edge.
  if (!up && this->disconnects_sensor_ != nullptr) {
    this->disconnects_++;
    this->disconnects_sensor_->publish_state(this->disconnects_);
  }
#endif
}

void UartTcp::read_socket_() {
  // A hardware write blocks until the driver takes every byte. Leave what does
  // not fit in the socket, so TCP flow control throttles the peer.
  size_t room = this->parent_->paced_write_room(this->last_write_ms_);
  if (room == 0) {
    // The UART cannot take more. The peer is not idle.
    this->rx_pending_ = true;
    this->link_.note_io();
    return;
  }
  uint8_t tmp[READ_CHUNK];
  size_t want = std::min(room, sizeof(tmp));
#ifdef USE_NOISE_STREAM
  ssize_t count = this->noise_ != nullptr ? this->noise_->read(this->link_, tmp, want) : this->link_.read(tmp, want);
#else
  ssize_t count = this->link_.read(tmp, want);
#endif
  if (count <= 0) {
    // A dropped link (-1) is cleaned up by sync_link_() on the next loop.
    if (count == 0) {
      this->rx_pending_ = false;
    }
    return;
  }
  this->rx_pending_ = static_cast<size_t>(count) == want;
  this->write_array(tmp, static_cast<size_t>(count));
  this->last_write_ms_ = App.get_loop_component_start_time();
}

void UartTcp::discard_uart_() {
  // Drain exactly what was buffered while the link was down; later bytes are live.
  uint8_t dump[DISCARD_CHUNK];
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
#ifdef USE_NOISE_STREAM
  if (this->noise_ != nullptr) {
    // Plaintext goes into the stream's open frame, so it cannot fill the link's buffer directly.
    uint8_t tmp[READ_CHUNK];
    for (;;) {
      size_t want = std::min({this->available(), this->noise_->tx_free(this->link_), sizeof(tmp)});
      if (want == 0 || !this->read_array(tmp, want)) {
        return;
      }
      this->noise_->queue(this->link_, tmp, want);
    }
  }
#endif
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
  bool up = this->link_up_();
  if (up != this->link_was_up_) {
    this->sync_link_(up);
  }
  if (!this->link_was_up_) {
    return;
  }
  // A byte moved in this pass resets the clock before the timeout can close.
  if (this->rx_pending_ || this->link_.ready()) {
    this->read_socket_();
  }
  // UART bytes picked up here go out in the same pass.
  this->read_uart_();
  this->flush_link_();
  this->link_.check_idle();
}

}  // namespace esphome::uart_tcp
