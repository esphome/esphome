#include "tcp_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::tcp_uart {

ESPHOME_LOG_TAG(TAG, "tcp_uart");

static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;
#ifdef USE_TCP_UART_MAX_CONNECTIONS
// A client whose reply takes no byte this long while another client waits is closed. Longer than a few Wi-Fi
// retransmissions, which can stall the send for seconds.
static constexpr uint32_t TX_STALL_TIMEOUT_MS = 5000;
#endif

#ifdef USE_TCP_UART_MAX_CONNECTIONS
void TcpUart::set_max_connections(uint8_t count) {
  this->extra_.init(count - 1);
  for (uint8_t i = 1; i < count; i++) {
    this->extra_.emplace_back().set_tx_buffer(this->tx_);
  }
  // The first search for a holder starts at slot 0.
  this->last_holder_ = count - 1;
}
#endif

void TcpUart::setup() {
  this->link_.begin(TAG);
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  for (auto &link : this->extra_) {
    link.begin(TAG);
    link.set_idle_timeout(this->link_.idle_timeout());
  }
#endif
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.begin(TAG);
#endif
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->connected_binary_sensor_, false);
#endif
#ifdef USE_SENSOR
  PUBLISH_STATE(this->disconnects_sensor_, 0);
#endif
}

void TcpUart::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TCP UART:\n"
                "  %s: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms\n"
                "  Timeout: %" PRIu32 "ms",
                this->server_ ? LOG_STR_LITERAL("Listen") : LOG_STR_LITERAL("Host"),
                this->server_ ? LOG_STR_LITERAL("*") : this->link_.host(), this->link_.port(),
                this->link_.reconnect_interval(), this->link_.idle_timeout());
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  if (!this->extra_.empty()) {
    ESP_LOGCONFIG(TAG,
                  "  Max Connections: %u\n"
                  "  Turn Timeout: %ums",
                  this->slot_count_(), this->turn_timeout_ms_);
  }
#endif
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.dump_config();
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Disconnects", this->disconnects_sensor_);
#endif
}

void TcpUart::on_shutdown() {
  this->link_.close();
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  for (auto &link : this->extra_) {
    link.close();
  }
#endif
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.close();
#endif
}

bool TcpUart::is_connected() {
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  for (auto &link : this->extra_) {
    if (link.connected()) {
      return true;
    }
  }
#endif
  return this->link_.connected();
}

void TcpUart::publish_connected_(bool up) {
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->connected_binary_sensor_, up);
#endif
}

void TcpUart::count_disconnect_() {
#ifdef USE_SENSOR
  if (this->disconnects_sensor_ != nullptr) {
    this->disconnects_++;
    this->disconnects_sensor_->publish_state(this->disconnects_);
  }
#endif
}

void TcpUart::sync_link_() {
  bool up = this->link_.connected();
  this->link_was_up_ = up;
  // Each session is a turn of its own.
  this->turn_++;
  // A hold belongs to the session that took it.
  this->held_ = false;
  if (up) {
    // Unread bytes of the last session stay readable while down, never into the next one.
    this->rx_start_ = this->rx_end_ = 0;
  }
  this->publish_connected_(up);
  // Only edges get here, so down is the falling edge.
  if (!up) {
    this->count_disconnect_();
  }
}

size_t TcpUart::read_socket_(Link &link, size_t limit) {
  if (this->rx_start_ != 0) {
    this->rx_end_ -= this->rx_start_;
    std::memmove(this->rx_, this->rx_ + this->rx_start_, this->rx_end_);
    this->rx_start_ = 0;
  }
  size_t room = std::min(RX_BUFFER_SIZE - this->rx_end_, limit);
  if (room == 0) {
    // Unread bytes are waiting on the consumer. The peer is not idle.
    link.note_io();
    return 0;
  }
  ssize_t count = link.read(this->rx_ + this->rx_end_, room);
  if (count <= 0) {
    // A dropped link (-1) is seen by the edge check on the next loop.
    if (count == 0) {
      this->rx_pending_ = false;
    }
    return 0;
  }
  this->rx_end_ += static_cast<uint16_t>(count);
  this->rx_pending_ = static_cast<size_t>(count) == room;
  return static_cast<size_t>(count);
}

void TcpUart::loop() {
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  if (!this->extra_.empty()) {
    // Inline until a client connects.
    bool waiting = this->listener_.poll_listen(this->link_, this->slot_count_());
    if (waiting || this->up_mask_ != 0) {
      this->loop_pool_(waiting);
    }
    return;
  }
#endif
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
    this->read_socket_(this->link_);
  }
  this->link_.flush_tx();
  if (this->held_) {
    // The peer waits for its reply.
    this->link_.note_io();
  }
  this->link_.check_idle();
}

#ifdef USE_TCP_UART_MAX_CONNECTIONS
void TcpUart::loop_pool_(bool waiting) {
  const uint32_t now = App.get_loop_component_start_time();
  const uint8_t count = this->slot_count_();
  for (uint8_t i = 0; i < count; i++) {
    if (this->slot_(i).connected() != ((this->up_mask_ & (1u << i)) != 0)) {
      this->slot_edge_(i);
    }
  }
  if (waiting) {
    this->accept_slot_(now);
  }
  if (this->holder_ != NO_SLOT) {
    // After release_turn() only the bytes already read are left to consume, and the rest of a request started in them.
    const bool rest = this->rest_ != 0 && !this->held_ && this->available() == 0;
    if ((!this->release_ || rest) && (this->rx_pending_ || this->slot_(this->holder_).ready())) {
      this->read_holder_(now);
    }
    if (this->available() == 0 && !this->held_ &&
        ((this->release_ && this->rest_ == 0) || now - this->last_traffic_ms_ >= this->turn_timeout_ms_)) {
      this->end_turn_(now);
    }
  }
  if (this->holder_ == NO_SLOT) {
    this->pick_holder_(now);
  }
  for (uint8_t i = 0; i < count; i++) {
    Link &link = this->slot_(i);
    link.flush_tx();
    // Bytes still waiting in its socket, for their turn or for room, are not a quiet peer. Nor is a held turn's peer:
    // it waits for its reply.
    const bool busy =
        (this->pending_mask_ & (1u << i)) != 0 || (i == this->holder_ && (this->rx_pending_ || this->held_));
    if (busy || link.ready()) {
      link.note_io();
    }
    link.check_idle();
  }
}

void TcpUart::slot_edge_(uint8_t i) {
  const uint8_t bit = 1u << i;
  const bool was_up = this->up_mask_ != 0;
  if (this->slot_(i).connected()) {
    this->up_mask_ |= bit;
  } else {
    this->up_mask_ &= ~bit;
    this->pending_mask_ &= ~bit;
    if (i == this->holder_) {
      // Its unread bytes never reach another connection.
      this->rx_start_ = this->rx_end_ = 0;
      this->rx_pending_ = false;
      this->holder_ = NO_SLOT;
    }
    if (i == this->tx_slot_) {
      // The holder or the last holder closed: the turn that writes belong to is over.
      this->tx_slot_ = NO_SLOT;
      this->turn_++;
    }
    this->count_disconnect_();
  }
  if ((this->up_mask_ != 0) != was_up) {
    this->publish_connected_(!was_up);
  }
}

void TcpUart::accept_slot_(uint32_t now) {
  auto sock = this->listener_.accept(this->link_);
  if (sock == nullptr) {
    return;
  }
  const uint8_t count = this->slot_count_();
  uint8_t slot = NO_SLOT;
  uint8_t quietest = NO_SLOT;
  uint32_t quietest_idle = 0;
  for (uint8_t i = 0; i < count; i++) {
    Link &link = this->slot_(i);
    if (!link.connected()) {
      slot = i;
      break;
    }
    if (i == this->holder_) {
      continue;
    }
    // A connection with bytes waiting for its turn is not idle.
    const bool waiting = (this->pending_mask_ & (1u << i)) != 0 || link.ready();
    const uint32_t idle = waiting ? 0 : now - link.last_io();
    if (quietest == NO_SLOT || idle > quietest_idle) {
      quietest = i;
      quietest_idle = idle;
    }
  }
  if (slot == NO_SLOT) {
    if (quietest == NO_SLOT) {
      return;
    }
    // A full pool closes the quietest connection other than the holder. One waiting for its turn is closed only when
    // all others wait too; Modbus TCP [4.2.1.1] asks for the oldest unused one.
    ESP_LOGI(TAG, "All %u connections in use, closing the quietest", count);
    slot = quietest;
    this->slot_(slot).close();
    this->slot_edge_(slot);
  }
  this->slot_(slot).adopt(std::move(sock));
  this->slot_edge_(slot);
}

void TcpUart::pick_holder_(uint32_t now) {
  // The connections share one TX buffer: the turn moves once the last holder's reply has left it.
  if (this->tx_slot_ != NO_SLOT && !this->tx_drained_(now)) {
    return;
  }
  const uint8_t count = this->slot_count_();
  for (uint8_t step = 1; step <= count; step++) {
    const uint8_t i = (this->last_holder_ + step) % count;
    Link &link = this->slot_(i);
    const uint8_t bit = 1u << i;
    if (!link.connected() || ((this->pending_mask_ & bit) == 0 && !link.ready())) {
      continue;
    }
    this->holder_ = i;
    this->held_ = false;
    this->release_ = false;
    this->rest_ = 0;
    this->rx_start_ = this->rx_end_ = 0;
    this->rx_pending_ = (this->pending_mask_ & bit) != 0;
    this->pending_mask_ &= ~bit;
    this->last_traffic_ms_ = now;
    this->read_holder_(now);
    if (this->available() == 0 && !this->rx_pending_) {
      // A read that stopped exactly at the rest of a request marks bytes pending that may not be there: no empty turn.
      this->holder_ = NO_SLOT;
      continue;
    }
    this->tx_slot_ = i;
    this->turn_++;
    return;
  }
}

bool TcpUart::tx_drained_(uint32_t now) {
  Link &link = this->slot_(this->tx_slot_);
  if (link.flush_tx()) {
    // Bytes that reach the last holder after the buffer emptied get a stall clock of their own.
    this->tx_moved_ms_ = now;
    return true;
  }
  const uint16_t room = static_cast<uint16_t>(link.tx_free());
  if (room != this->tx_free_seen_) {
    this->tx_free_seen_ = room;
    this->tx_moved_ms_ = now;
    return false;
  }
  if (now - this->tx_moved_ms_ < TX_STALL_TIMEOUT_MS) {
    return false;
  }
  const uint8_t count = this->slot_count_();
  for (uint8_t i = 0; i < count; i++) {
    Link &other = this->slot_(i);
    if (i == this->tx_slot_ || !other.connected() || ((this->pending_mask_ & (1u << i)) == 0 && !other.ready())) {
      continue;
    }
    // Another client waits. The rest of this reply cannot go to another connection, so closing is the only way out.
    ESP_LOGW(TAG, "Connection took no bytes for %" PRIu32 "ms while another waits, closing", now - this->tx_moved_ms_);
    link.close();
    this->slot_edge_(this->tx_slot_);
    return true;
  }
  return false;
}

void TcpUart::read_holder_(uint32_t now) {
  // A released turn reads no more than the rest that release_turn() named.
  const size_t count = this->read_socket_(this->slot_(this->holder_), this->release_ ? this->rest_ : RX_BUFFER_SIZE);
  if (count != 0) {
    this->last_traffic_ms_ = now;
    if (this->release_) {
      this->rest_ -= static_cast<uint16_t>(count);
    }
  }
}

void TcpUart::end_turn_(uint32_t now) {
  // The stall clock of the reply in the TX buffer starts with the turn's end.
  this->tx_moved_ms_ = now;
  // Bytes left in the socket keep their place for the next turn.
  if (this->rx_pending_) {
    this->pending_mask_ |= 1u << this->holder_;
    this->rx_pending_ = false;
  }
  this->last_holder_ = this->holder_;
  this->holder_ = NO_SLOT;
  this->held_ = false;
  this->release_ = false;
  this->rest_ = 0;
}
#endif

void TcpUart::write_array(const uint8_t *data, size_t len) {
  Link *link = this->tx_link_();
  size_t queued = link != nullptr ? link->queue(data, len) : 0;
#ifdef USE_TCP_UART_MAX_CONNECTIONS
  if (queued != 0 && this->holder_ != NO_SLOT) {
    this->last_traffic_ms_ = App.get_loop_component_start_time();
  }
#endif
  if (queued < len) {
    uint32_t now = App.get_loop_component_start_time();
    if (this->last_drop_log_ms_ == 0 || now - this->last_drop_log_ms_ >= DROP_LOG_INTERVAL_MS) {
      // With clients open, writes find no connection before the first turn and after the last holder closed.
      ESP_LOGW(TAG, "%s, dropped %u bytes",
               link != nullptr && link->connected() ? LOG_STR_LITERAL("TX buffer full")
               : this->is_connected()               ? LOG_STR_LITERAL("No connection holds the turn")
                                                    : LOG_STR_LITERAL("Not connected"),
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
  Link *link = this->tx_link_();
  if (link == nullptr) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  bool emptied = link->flush_tx();
  if (!link->connected()) {
    // A down link cannot have delivered anything, whether this flush dropped
    // it or an earlier loop() write did.
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  return emptied ? uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS : uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
}

}  // namespace esphome::tcp_uart
