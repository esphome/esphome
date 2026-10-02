#include "uart_tcp.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::uart_tcp {

static const char *const TAG = "uart_tcp";

// One client at a time; a second connection waits in the stack until the first drops.
static constexpr int LISTEN_BACKLOG = 1;
// Bytes per 16 ms loop pass at 10 bits per byte: baud / 10 / 62.5.
static constexpr uint32_t BAUD_PACE_DIVISOR = 625;

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
  } else {
    // The driver kept whatever arrived while the link was down.
    this->discard_uart_();
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
      this->listen_->listen(LISTEN_BACKLOG) != 0) {
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
    // link_was_up_ holds the accept until the previous drop's edge has run,
    // so the old client's buffered bytes never reach the new one.
    if (this->listen_ != nullptr && !this->link_.connected() && !this->link_was_up_ && this->listen_->ready()) {
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
