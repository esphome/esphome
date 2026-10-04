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
// A peer sends a whole frame and then waits. After this much quiet the next byte starts a frame.
static constexpr uint32_t RESYNC_QUIET_US = 100000;
// An exception reply sets the top bit of the request's function code.
static constexpr uint8_t FUNCTION_CODE_MASK = 0x7F;

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
  ESP_LOGCONFIG(TAG,
                "Modbus TCP:\n"
                "  Role: %s",
                this->server_ ? LOG_STR_LITERAL("server") : LOG_STR_LITERAL("client"));
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
    this->resync_ = false;
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
  if (this->tcp_len_ != 0) {
    this->deliver_mbap_();
  }
  if (this->resync_) {
    if (this->parent_->available() != 0) {
      this->discard_parent_();
      this->resync_from_us_ = micros();
      return;
    }
    if (micros() - this->resync_from_us_ < RESYNC_QUIET_US) {
      return;
    }
    this->resync_ = false;
  }
  size_t room = sizeof(this->tcp_buf_) - this->tcp_len_;
  // Any frame fits, so a full buffer starts with a whole frame that waits for the hub. Leave it.
  if (room == 0) {
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
  // frame.pdu points into tcp_buf_. Copy it out before the tail slides.
  uint16_t pos = 0;
  while (pos < this->tcp_len_) {
    Mbap frame;
    size_t used = 0;
    size_t left = static_cast<size_t>(this->tcp_len_ - pos);
    switch (take_mbap(this->tcp_buf_ + pos, left, &frame, &used)) {
      case MbapTake::NEED_MORE:
        used = 0;
        break;
      case MbapTake::BAD: {
        // [TCP 4.4.2.2] discards only the bad frame. A usable length says where it ends.
        size_t size = mbap_announced_size(this->tcp_buf_ + pos, left);
        if (size == 0) {
          // No frame boundary. A byte-wise search could hand the hub a mid-frame match with a fresh CRC.
          note_drop(this->drop_log_ms_[DROP_BAD_MBAP],
                    LOG_STR("Invalid MBAP length, dropped until the stream is quiet"));
          this->tcp_len_ = 0;
          this->resync_ = true;
          this->resync_from_us_ = micros();
          return;
        }
        // Any frame fits the buffer. Wait for all of it, then skip it.
        used = size <= left ? size : 0;
        if (used != 0) {
          note_drop(this->drop_log_ms_[DROP_BAD_MBAP], LOG_STR("Invalid MBAP protocol id, frame skipped"));
        }
        break;
      }
      case MbapTake::FRAME: {
        if (this->server_) {
          // One request at a time. This one stays here until the hub has read the last one and its reply went out,
          // or the reply is overdue.
          uint32_t now = App.get_loop_component_start_time();
          if (this->rx_len_ != 0 || (this->txn_pending_ && now - this->request_ms_ < REPLY_TIMEOUT_MS)) {
            used = 0;
            break;
          }
          // No reply in time. The next request takes its place.
          if (this->txn_pending_) {
            if (this->tx_len_ != 0) {
              note_drop(this->drop_log_ms_[DROP_REPLACED], LOG_STR("Unanswered request replaced"));
              this->clear_tx_();
            }
            this->txn_pending_ = false;
          }
          // The hub leaves a request for another unit to the RTU device that has it, and takes the next frame as
          // that device's reply. No such device is on this link, so the next request would be lost. Drop it here.
          const uint8_t *units_end = this->units_ + this->units_count_;
          if (frame.unit != 0 && this->units_ != nullptr &&
              std::find(this->units_, units_end, frame.unit) == units_end) {
            ESP_LOGV(TAG, "No server for unit %u, request dropped", frame.unit);
            break;
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
            this->unit_ = frame.unit;
            this->function_ = frame.pdu[0];
            this->request_ms_ = now;
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
    // A late reply to an earlier request, or one from another unit, must not take this request's id.
    if (!this->txn_pending_ || this->tx_[0] != this->unit_ || (this->tx_[1] & FUNCTION_CODE_MASK) != this->function_) {
      note_drop(this->drop_log_ms_[DROP_NO_REQUEST], LOG_STR("Reply without a matching request, dropped"));
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
