#include "modbus_tcp_uart.h"

#include "mbap.h"
#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::modbus_tcp_uart {

static const char *const TAG = "modbus_tcp_uart";

static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;
// A peer sends a whole frame and then waits. After this much quiet the next byte starts a frame.
static constexpr uint32_t RESYNC_QUIET_US = 100000;

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

void ModbusTcpUart::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Modbus TCP UART:\n"
                "  Role: %s",
                this->server_ ? LOG_STR_LITERAL("server") : LOG_STR_LITERAL("client"));
}

bool ModbusTcpUart::is_connected() { return this->parent_ != nullptr && this->parent_->is_connected(); }

void ModbusTcpUart::clear_tx_() {
  this->tx_len_ = 0;
  this->tx_frame_len_ = 0;
  this->tx_hold_logged_ = false;
}

void ModbusTcpUart::consume_tx_(size_t len) {
  this->tx_len_ = static_cast<uint16_t>(this->tx_len_ - len);
  std::memmove(this->tx_, this->tx_ + len, this->tx_len_);
  this->tx_frame_len_ = 0;
  this->tx_hold_logged_ = false;
}

void ModbusTcpUart::loop() {
  bool up = this->is_connected();
  if (!up) {
    if (this->link_was_up_ &&
        (this->txn_pending_ || this->tx_len_ != 0 || this->available() != 0 || this->tcp_len_ != 0)) {
      ESP_LOGW(TAG, "%s", LOG_STR_ARG(LOG_STR("Link down, dropped the Modbus frame in flight")));
    }
    this->link_was_up_ = false;
    this->resync_ = false;
    this->tcp_len_ = 0;
    this->clear_tx_();
    this->rx_.clear();
    this->txn_pending_ = false;
    return;
  }
  this->link_was_up_ = true;
  if (this->tx_frame_len_ != 0) {
    this->send_tx_(0);
  }
  this->read_parent_();
}

void ModbusTcpUart::write_array(const uint8_t *data, size_t len) {
  // The new request is still buffered, so this write answers the previous one.
  if (this->server_ && this->available() != 0) {
    note_drop(this->drop_log_ms_[DROP_REPLY_PREVIOUS], LOG_STR("Reply to the previous request, dropped"));
    return;
  }
  // A whole frame is only still here because the transport could not take it.
  // The writer has moved on, so the next write replaces it.
  if (this->tx_frame_len_ != 0) {
    note_drop(this->drop_log_ms_[DROP_HELD], LOG_STR("Dropping a held Modbus frame"));
    this->consume_tx_(this->tx_frame_len_);
  }
  // A part from earlier cannot go on with a write that does not fit or that starts with a whole frame.
  size_t frame_len = 0;
  if (this->tx_len_ != 0 &&
      (this->tx_len_ + len > sizeof(this->tx_) || take_rtu(data, len, this->server_, &frame_len) == RtuTake::FRAME)) {
    note_drop(this->drop_log_ms_[DROP_INCOMPLETE], LOG_STR("Part of a Modbus frame dropped"));
    this->tx_len_ = 0;
  }
  if (len > sizeof(this->tx_)) {
    note_drop(this->drop_log_ms_[DROP_INCOMPLETE], LOG_STR("Not a Modbus frame, dropped"));
    return;
  }
  const size_t held = this->tx_len_;
  std::memcpy(this->tx_ + held, data, len);
  this->tx_len_ = static_cast<uint16_t>(held + len);
  this->send_tx_(held);
}

void ModbusTcpUart::send_tx_(size_t held) {
  while (this->tx_len_ != 0) {
    if (this->tx_frame_len_ == 0) {
      size_t frame_len = 0;
      const RtuTake take = take_rtu(this->tx_, this->tx_len_, this->server_, &frame_len);
      if (take == RtuTake::NEED_MORE) {
        return;
      }
      if (take == RtuTake::BAD) {
        // The part from earlier and the last write do not make a frame: try the write on its own.
        note_drop(this->drop_log_ms_[DROP_INCOMPLETE],
                  held != 0 ? LOG_STR("Part of a Modbus frame dropped") : LOG_STR("Not a Modbus frame, dropped"));
        if (held == 0) {
          this->tx_len_ = 0;
          return;
        }
        this->consume_tx_(held);
        held = 0;
        continue;
      }
      this->tx_frame_len_ = static_cast<uint16_t>(frame_len);
      held = held > frame_len ? held - frame_len : 0;
    }
    this->send_rtu_as_mbap_();
    if (this->tx_frame_len_ != 0) {
      return;
    }
  }
}

size_t ModbusTcpUart::available_for_write() {
  // Anything written now would replace a whole frame that still waits for the transport.
  if (!this->is_connected() || this->tx_frame_len_ != 0) {
    return 0;
  }
  return sizeof(this->tx_) - this->tx_len_;
}

uart::UARTFlushResult ModbusTcpUart::flush() {
  if (this->tx_frame_len_ != 0) {
    this->send_tx_(0);
  }
  // A whole frame is still here, so the writer must not be told that the flush finished.
  if (this->parent_ == nullptr || this->tx_frame_len_ != 0) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  return this->parent_->flush();
}

void ModbusTcpUart::read_parent_() {
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

void ModbusTcpUart::deliver_mbap_() {
  static_assert(RTU_FRAME_SIZE >= MBAP_MAX_LENGTH + 2, "the PDU of any MBAP frame fits one RTU frame");
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
          if (this->available() != 0 || (this->txn_pending_ && now - this->request_ms_ < REPLY_TIMEOUT_MS)) {
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
          uint8_t rtu[RTU_FRAME_SIZE];
          size_t rtu_len = write_rtu(rtu, frame.unit, frame.pdu, frame.pdu_len);
          // Address 0 is a broadcast. Nothing answers it.
          if (frame.unit != 0) {
            this->txn_ = frame.txn;
            this->unit_ = frame.unit;
            this->function_ = frame.pdu[0];
            this->request_ms_ = now;
            this->txn_pending_ = true;
          }
          // Last: an attached reader may answer within this call.
          this->inject_rx(rtu, rtu_len);
          break;
        }
        if (!this->txn_pending_ || frame.txn != this->txn_) {
          if (drop_log_due(this->drop_log_ms_[DROP_STALE])) {
            ESP_LOGW(TAG, "Dropped transaction %u, expected %u", frame.txn, this->txn_);
          }
          break;
        }
        // [TCP 4.4.1.3] The client discards the response's unit. The hub expects the address it asked.
        uint8_t rtu[RTU_FRAME_SIZE];
        size_t rtu_len = write_rtu(rtu, this->unit_, frame.pdu, frame.pdu_len);
        // Before handing it on: an attached reader may send the next request within the call.
        this->txn_pending_ = false;
        if (!this->inject_rx(rtu, rtu_len)) {
          note_drop(this->drop_log_ms_[DROP_RX_FULL], LOG_STR("RX buffer full, dropped the response"));
        }
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

// Sends the whole frame at the front of tx_, or keeps it there while the transport has no room.
void ModbusTcpUart::send_rtu_as_mbap_() {
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
    if (!this->txn_pending_ || this->tx_[0] != this->unit_ ||
        (this->tx_[1] & modbus::FUNCTION_CODE_MASK) != this->function_) {
      note_drop(this->drop_log_ms_[DROP_NO_REQUEST], LOG_STR("Reply without a matching request, dropped"));
      this->consume_tx_(this->tx_frame_len_);
      return;
    }
    txn = this->txn_;
  } else {
    txn = this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
  }
  uint8_t frame[TCP_FRAME_SIZE];
  size_t n = write_mbap(frame, sizeof(frame), txn, this->tx_[0], this->tx_ + 1, this->tx_frame_len_ - 3);
  if (n == 0) {
    note_drop(this->drop_log_ms_[DROP_ENCODE], LOG_STR("Cannot encode the Modbus frame, dropped"));
    this->consume_tx_(this->tx_frame_len_);
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
    this->unit_ = this->tx_[0];
    this->txn_pending_ = true;
  }
  this->consume_tx_(this->tx_frame_len_);
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

void ModbusTcpUart::discard_parent_() {
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

}  // namespace esphome::modbus_tcp_uart
