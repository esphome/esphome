#include "modbus_tcp.h"

#include "mbap.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::modbus_tcp {

static const char *const TAG = "modbus_tcp";

static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;
// A bridge copies every loop. A pause this long is an unfinished frame, not the next chunk.
static constexpr uint32_t TX_PARTIAL_STALE_MS = 300;

static bool drop_log_due(uint32_t &last_ms) {
  uint32_t now = App.get_loop_component_start_time();
  if (last_ms != 0 && now - last_ms < DROP_LOG_INTERVAL_MS) {
    return false;
  }
  // A zero stamp would look like "never logged" on the next pass.
  last_ms = now == 0 ? 1 : now;
  return true;
}

static void note_drop(uint32_t &last_ms, const LogString *message) {
  if (!drop_log_due(last_ms)) {
    return;
  }
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(message));
}

void ModbusTcp::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus TCP");
  ESP_LOGCONFIG(TAG, "  Role: %s", this->server_ ? LOG_STR_LITERAL("server") : LOG_STR_LITERAL("client"));
}

bool ModbusTcp::is_connected() { return this->parent_ != nullptr && this->parent_->is_connected(); }

void ModbusTcp::clear_tx_() {
  this->tx_len_ = 0;
  this->tx_hold_logged_ = false;
}

void ModbusTcp::loop() {
  bool up = this->is_connected();
  if (!up) {
    if (this->link_was_up_ && (this->txn_pending_ || this->tx_len_ != 0 || this->rx_len_ != 0 || this->tcp_len_ != 0)) {
      ESP_LOGW(TAG, "%s", LOG_STR_ARG(LOG_STR("Link down, dropped the Modbus frame in flight")));
    }
    this->link_was_up_ = false;
    this->drop_until_down_ = false;
    this->tcp_len_ = 0;
    this->clear_tx_();
    this->rx_len_ = 0;
    this->txn_pending_ = false;
    return;
  }
  this->link_was_up_ = true;
  if (this->tx_len_ != 0 && !rtu_crc_ok(this->tx_, this->tx_len_)) {
    uint32_t now = App.get_loop_component_start_time();
    if (now - this->tx_partial_ms_ >= TX_PARTIAL_STALE_MS) {
      note_drop(this->drop_log_ms_[DROP_INCOMPLETE], LOG_STR("Incomplete Modbus frame dropped"));
      this->clear_tx_();
    }
  }
  if (this->tx_len_ != 0) {
    this->send_rtu_as_mbap_();
  }
  this->read_parent_();
}

void ModbusTcp::write_array(const uint8_t *data, size_t len) {
  // The new request is still buffered, so this write answers the previous one.
  if (this->server_ && this->rx_len_ != 0) {
    note_drop(this->drop_log_ms_[DROP_REPLY_PREVIOUS], LOG_STR("Reply to the previous request, dropped"));
    return;
  }
  // A complete frame is only still here because the transport could not take it.
  // The hub has moved on, so the next write replaces it instead of appending.
  // An unfinished frame is not a prefix of the next one. A whole frame, or a pause, drops it.
  uint32_t now = App.get_loop_component_start_time();
  if (this->tx_len_ != 0 && rtu_crc_ok(this->tx_, this->tx_len_)) {
    note_drop(this->drop_log_ms_[DROP_HELD], LOG_STR("Dropping a held Modbus frame"));
    this->clear_tx_();
  } else if (this->tx_len_ != 0 && (rtu_crc_ok(data, len) || now - this->tx_partial_ms_ >= TX_PARTIAL_STALE_MS)) {
    note_drop(this->drop_log_ms_[DROP_INCOMPLETE], LOG_STR("Incomplete Modbus frame dropped"));
    this->clear_tx_();
  }
  size_t before = this->tx_len_;
  size_t room = sizeof(this->tx_) - this->tx_len_;
  size_t n = std::min(len, room);
  std::memcpy(this->tx_ + this->tx_len_, data, n);
  this->tx_len_ += static_cast<uint16_t>(n);
  this->tx_partial_ms_ = now;
  if (n < len) {
    note_drop(this->drop_log_ms_[DROP_TOO_LONG], LOG_STR("RTU frame too long, dropped"));
    this->clear_tx_();
    return;
  }
  // The hub writes one whole RTU frame and does not flush unless a flow-control
  // pin is set. A frame that arrives in that one write already carries its CRC.
  if (before == 0 && rtu_crc_ok(this->tx_, this->tx_len_)) {
    this->send_rtu_as_mbap_();
  }
}

bool ModbusTcp::peek_byte(uint8_t *data) {
  if (this->rx_len_ == 0) {
    return false;
  }
  *data = this->rx_[0];
  return true;
}

bool ModbusTcp::read_array(uint8_t *data, size_t len) {
  if (this->available() < len) {
    return false;
  }
  std::memcpy(data, this->rx_, len);
  this->rx_len_ = static_cast<uint16_t>(this->rx_len_ - len);
  if (this->rx_len_ != 0) {
    std::memmove(this->rx_, this->rx_ + len, this->rx_len_);
  }
  return true;
}

size_t ModbusTcp::available_for_write() {
  if (!this->is_connected()) {
    return 0;
  }
  return sizeof(this->tx_) - this->tx_len_;
}

uart::UARTFlushResult ModbusTcp::flush() {
  this->send_rtu_as_mbap_();
  // The frame is still here, so the hub must not be told that the flush finished.
  if (this->parent_ == nullptr || this->tx_len_ != 0) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  return this->parent_->flush();
}

void ModbusTcp::read_parent_() {
  if (this->drop_until_down_) {
    this->discard_parent_();
    return;
  }
  if (this->tcp_len_ != 0) {
    this->deliver_mbap_();
  }
  if (this->drop_until_down_) {
    this->discard_parent_();
    return;
  }
  size_t room = sizeof(this->tcp_buf_) - this->tcp_len_;
  if (room == 0) {
    Mbap frame;
    size_t used = 0;
    // A whole frame is waiting for the hub. Leave it, and do not read more.
    if (take_mbap(this->tcp_buf_, this->tcp_len_, &frame, &used) == MbapTake::FRAME) {
      return;
    }
    this->drop_stream_();
    this->discard_parent_();
    return;
  }
  size_t have = this->parent_->available();
  if (have == 0) {
    return;
  }
  size_t n = std::min(room, have);
  if (!this->parent_->read_array(this->tcp_buf_ + this->tcp_len_, n)) {
    note_drop(this->drop_log_ms_[DROP_READ], LOG_STR("Read failed"));
    return;
  }
  this->tcp_len_ += static_cast<uint16_t>(n);
  this->deliver_mbap_();
}

void ModbusTcp::deliver_mbap_() {
  if (this->drop_until_down_) {
    this->tcp_len_ = 0;
    return;
  }
  // frame.pdu points into tcp_buf_. Copy it out before the tail slides.
  uint16_t pos = 0;
  while (pos < this->tcp_len_) {
    Mbap frame;
    size_t used = 0;
    switch (take_mbap(this->tcp_buf_ + pos, static_cast<size_t>(this->tcp_len_ - pos), &frame, &used)) {
      case MbapTake::NEED_MORE:
        used = 0;
        break;
      case MbapTake::BAD:
        // No resync. A fresh CRC would make the hub accept bytes from the middle of a frame.
        this->drop_stream_();
        return;
      case MbapTake::FRAME: {
        if (this->server_) {
          // The hub has not read the last request. Leave this one where it is.
          if (this->rx_len_ != 0) {
            used = 0;
            break;
          }
          // Handed on, and nothing has been sent back. The next request takes its place.
          if (this->txn_pending_) {
            if (this->tx_len_ != 0) {
              note_drop(this->drop_log_ms_[DROP_REPLACED], LOG_STR("Unanswered request replaced"));
              this->clear_tx_();
            }
            this->txn_pending_ = false;
          }
          size_t rtu_len = frame.pdu_len + 3;
          if (rtu_len > sizeof(this->rx_)) {
            note_drop(this->drop_log_ms_[DROP_TOO_LONG], LOG_STR("RTU frame too long, dropped"));
            break;
          }
          this->rx_[0] = frame.unit;
          std::memcpy(this->rx_ + 1, frame.pdu, frame.pdu_len);
          uint16_t crc = crc16(this->rx_, static_cast<uint16_t>(frame.pdu_len + 1));
          this->rx_[frame.pdu_len + 1] = crc & 0xFF;
          this->rx_[frame.pdu_len + 2] = crc >> 8;
          this->rx_len_ = static_cast<uint16_t>(rtu_len);
          // Address 0 is a broadcast. Nothing answers it.
          if (frame.unit != 0) {
            this->txn_ = frame.txn;
            this->txn_pending_ = true;
          }
          break;
        }
        if (!this->txn_pending_ || frame.txn != this->txn_) {
          if (drop_log_due(this->drop_log_ms_[DROP_STALE])) {
            ESP_LOGW(TAG, "Dropped transaction %u, expected %u", frame.txn, this->txn_);
          }
          break;
        }
        size_t rtu_len = frame.pdu_len + 3;
        if (static_cast<size_t>(this->rx_len_) + rtu_len > sizeof(this->rx_)) {
          note_drop(this->drop_log_ms_[DROP_RX_FULL], LOG_STR("RX buffer full, dropped the response"));
          break;
        }
        size_t at = this->rx_len_;
        this->rx_[at] = frame.unit;
        std::memcpy(this->rx_ + at + 1, frame.pdu, frame.pdu_len);
        uint16_t crc = crc16(this->rx_ + at, static_cast<uint16_t>(frame.pdu_len + 1));
        this->rx_[at + frame.pdu_len + 1] = crc & 0xFF;
        this->rx_[at + frame.pdu_len + 2] = crc >> 8;
        this->rx_len_ = static_cast<uint16_t>(at + rtu_len);
        this->txn_pending_ = false;
        break;
      }
    }
    if (used == 0) {
      break;
    }
    pos = static_cast<uint16_t>(pos + used);
  }
  if (pos == 0) {
    return;
  }
  this->tcp_len_ = static_cast<uint16_t>(this->tcp_len_ - pos);
  if (this->tcp_len_ != 0) {
    std::memmove(this->tcp_buf_, this->tcp_buf_ + pos, this->tcp_len_);
  }
}

void ModbusTcp::send_rtu_as_mbap_() {
  if (!rtu_crc_ok(this->tx_, this->tx_len_)) {
    return;
  }
  if (!this->is_connected()) {
    note_drop(this->drop_log_ms_[DROP_NOT_CONNECTED], LOG_STR("Not connected, dropped the Modbus frame"));
    this->clear_tx_();
    if (this->server_) {
      this->txn_pending_ = false;
    }
    return;
  }
  uint16_t txn;
  if (this->server_) {
    if (!this->txn_pending_) {
      note_drop(this->drop_log_ms_[DROP_NO_REQUEST], LOG_STR("Reply without a request, dropped"));
      this->clear_tx_();
      return;
    }
    txn = this->txn_;
  } else {
    txn = this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
  }
  uint8_t frame[TCP_FRAME_SIZE];
  size_t n = write_mbap(frame, sizeof(frame), txn, this->tx_[0], this->tx_ + 1, this->tx_len_ - 3);
  if (n == 0) {
    note_drop(this->drop_log_ms_[DROP_ENCODE], LOG_STR("Cannot encode the Modbus frame, dropped"));
    this->clear_tx_();
    if (this->server_) {
      this->txn_pending_ = false;
    }
    return;
  }
  // A short queue would put a partial MBAP on the wire. Hold the RTU and retry.
  // Logged once per frame, and not through note_drop(), so a later drop is still visible.
  size_t free = this->parent_->available_for_write();
  if (free < n) {
    if (!this->tx_hold_logged_) {
      this->tx_hold_logged_ = true;
      ESP_LOGD(TAG, "%s", LOG_STR_ARG(LOG_STR("TX buffer full, holding the Modbus frame")));
    }
    return;
  }
  if (this->server_) {
    this->txn_pending_ = false;
  } else {
    this->txn_ = txn;
    this->txn_pending_ = true;
  }
  this->clear_tx_();
  this->parent_->write_array(frame, n);
  // tcp_uart may already have run this pass. Flush so the request leaves now.
  uart::UARTFlushResult sent = this->parent_->flush();
  // TIMEOUT means the bytes are still queued on a link that is up. They leave later.
  if (sent == uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED) {
    note_drop(this->drop_log_ms_[DROP_SEND], LOG_STR("Send failed"));
    if (!this->server_) {
      this->txn_pending_ = false;
    }
  }
}

void ModbusTcp::drop_stream_() {
  // tcp_uart can close this socket. It already does on shutdown, and does not
  // expose that call yet, so further bytes are discarded until the peer drops.
  note_drop(this->drop_log_ms_[DROP_BAD_MBAP], LOG_STR("Invalid MBAP, dropped until reconnect"));
  this->tcp_len_ = 0;
  this->drop_until_down_ = true;
}

void ModbusTcp::discard_parent_() {
  uint8_t junk[32];
  size_t left = this->parent_->available();
  while (left != 0) {
    size_t n = std::min(left, sizeof(junk));
    if (!this->parent_->read_array(junk, n)) {
      return;
    }
    left -= n;
  }
}

}  // namespace esphome::modbus_tcp
