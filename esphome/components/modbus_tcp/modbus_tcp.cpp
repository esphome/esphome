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

static void note_drop(uint32_t &last_ms, const LogString *message) {
  uint32_t now = App.get_loop_component_start_time();
  if (last_ms != 0 && now - last_ms < DROP_LOG_INTERVAL_MS) {
    return;
  }
  // A zero stamp would look like "never logged" on the next pass.
  last_ms = now == 0 ? 1 : now;
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(message));
}

void ModbusTcp::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus TCP");
  ESP_LOGCONFIG(TAG, "  Role: %s", this->server_ ? LOG_STR_LITERAL("server") : LOG_STR_LITERAL("client"));
}

bool ModbusTcp::is_connected() { return this->parent_ != nullptr && this->parent_->is_connected(); }

void ModbusTcp::loop() {
  if (!this->is_connected()) {
    this->tcp_len_ = 0;
    this->tx_len_ = 0;
    this->rx_len_ = 0;
    this->txn_pending_ = false;
    return;
  }
  if (this->tx_len_ != 0) {
    this->send_rtu_as_mbap_();
  }
  this->read_parent_();
}

void ModbusTcp::write_array(const uint8_t *data, size_t len) {
  // A complete frame is only still here because the transport could not take it.
  // The hub has moved on, so the next write replaces it instead of appending.
  if (this->tx_len_ != 0 && rtu_crc_ok(this->tx_, this->tx_len_)) {
    note_drop(this->last_drop_log_ms_, LOG_STR("Dropping a held Modbus frame"));
    this->tx_len_ = 0;
  }
  size_t before = this->tx_len_;
  size_t room = sizeof(this->tx_) - this->tx_len_;
  size_t n = std::min(len, room);
  std::memcpy(this->tx_ + this->tx_len_, data, n);
  this->tx_len_ += static_cast<uint16_t>(n);
  if (n < len) {
    note_drop(this->last_drop_log_ms_, LOG_STR("RTU frame too long, dropped"));
    this->tx_len_ = 0;
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
  if (this->parent_ == nullptr) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  return this->parent_->flush();
}

void ModbusTcp::read_parent_() {
  if (this->tcp_len_ != 0) {
    this->deliver_mbap_();
  }
  size_t room = sizeof(this->tcp_buf_) - this->tcp_len_;
  if (room == 0) {
    note_drop(this->last_drop_log_ms_, LOG_STR("TCP buffer full, dropped"));
    this->tcp_len_ = 0;
    return;
  }
  size_t have = this->parent_->available();
  if (have == 0) {
    return;
  }
  size_t n = std::min(room, have);
  if (!this->parent_->read_array(this->tcp_buf_ + this->tcp_len_, n)) {
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
    switch (take_mbap(this->tcp_buf_ + pos, static_cast<size_t>(this->tcp_len_ - pos), &frame, &used)) {
      case MbapTake::NEED_MORE:
        used = 0;
        break;
      case MbapTake::BAD:
        break;
      case MbapTake::FRAME: {
        if (this->server_) {
          // The hub has not read the last request. Do not splice a new one under it.
          if (this->rx_len_ != 0) {
            note_drop(this->last_drop_log_ms_, LOG_STR("Request already in progress, dropped"));
            break;
          }
          // Handed on, and nothing has been sent back. The next request takes its place.
          if (this->txn_pending_) {
            if (this->tx_len_ != 0) {
              note_drop(this->last_drop_log_ms_, LOG_STR("Unanswered request replaced"));
              this->tx_len_ = 0;
            }
            this->txn_pending_ = false;
          }
          size_t rtu_len = frame.pdu_len + 3;
          if (rtu_len > sizeof(this->rx_)) {
            note_drop(this->last_drop_log_ms_, LOG_STR("RTU frame too long, dropped"));
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
          uint32_t now = App.get_loop_component_start_time();
          if (this->last_drop_log_ms_ == 0 || now - this->last_drop_log_ms_ >= DROP_LOG_INTERVAL_MS) {
            this->last_drop_log_ms_ = now == 0 ? 1 : now;
            ESP_LOGW(TAG, "Dropped transaction %u, expected %u", frame.txn, this->txn_);
          }
          break;
        }
        size_t rtu_len = frame.pdu_len + 3;
        if (static_cast<size_t>(this->rx_len_) + rtu_len > sizeof(this->rx_)) {
          note_drop(this->last_drop_log_ms_, LOG_STR("RX buffer full, dropped the response"));
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
    note_drop(this->last_drop_log_ms_, LOG_STR("Not connected, dropped the Modbus frame"));
    this->tx_len_ = 0;
    if (this->server_) {
      this->txn_pending_ = false;
    }
    return;
  }
  uint16_t txn;
  if (this->server_) {
    if (!this->txn_pending_) {
      note_drop(this->last_drop_log_ms_, LOG_STR("Reply without a request, dropped"));
      this->tx_len_ = 0;
      return;
    }
    txn = this->txn_;
  } else {
    txn = this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
  }
  uint8_t frame[TCP_FRAME_SIZE];
  size_t n = write_mbap(frame, sizeof(frame), txn, this->tx_[0], this->tx_ + 1, this->tx_len_ - 3);
  if (n == 0) {
    this->tx_len_ = 0;
    if (this->server_) {
      this->txn_pending_ = false;
    }
    return;
  }
  // A short queue would put a partial MBAP on the wire. Hold the RTU and retry.
  size_t free = this->parent_->available_for_write();
  if (free < n) {
    note_drop(this->last_drop_log_ms_, LOG_STR("TX buffer full, holding the Modbus frame"));
    return;
  }
  if (this->server_) {
    this->txn_pending_ = false;
  } else {
    this->txn_ = txn;
    this->txn_pending_ = true;
  }
  this->tx_len_ = 0;
  this->parent_->write_array(frame, n);
  // tcp_uart may already have run this pass. Flush so the request leaves now.
  this->parent_->flush();
}

}  // namespace esphome::modbus_tcp
