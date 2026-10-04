#include "mbap_link.h"

#ifdef USE_MODBUS_TCP

#include "mbap.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::modbus {

static const char *const TAG = "modbus";

static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;

MbapLink::MbapLink(bool server) : role_(server ? Role::ROLE_SERVER : Role::ROLE_CLIENT) {}

void log_throttled(uint32_t &last_ms, const LogString *message) {
  uint32_t now = App.get_loop_component_start_time();
  if (last_ms != 0 && now - last_ms < DROP_LOG_INTERVAL_MS) {
    return;
  }
  last_ms = now == 0 ? 1 : now;
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(message));
}

void drain_uart(uart::UARTComponent *uart) {
  uint8_t junk[32];
  size_t left = uart->available();
  while (left != 0) {
    size_t n = std::min(left, sizeof(junk));
    if (!uart->read_array(junk, n)) {
      return;
    }
    left -= n;
  }
}

void MbapLink::note_txn_(uint16_t got) {
  uint32_t now = App.get_loop_component_start_time();
  if (this->drop_stale_ms_ != 0 && now - this->drop_stale_ms_ < DROP_LOG_INTERVAL_MS) {
    return;
  }
  this->drop_stale_ms_ = now == 0 ? 1 : now;
  ESP_LOGW(TAG, "Dropped transaction %" PRIu16 ", expected %" PRIu16, got, this->txn_);
}

void MbapLink::reset_() {
  this->tcp_len_ = 0;
  this->rtu_len_ = 0;
  this->held_len_ = 0;
  this->tx_sent_ = 0;
  this->txn_pending_ = false;
  this->skip_left_ = 0;
  this->resync_ = false;
  this->link_was_up_ = false;
}

void MbapLink::consume_(size_t used) {
  if (used > this->tcp_len_) {
    this->tcp_len_ = 0;
    return;
  }
  this->tcp_len_ = static_cast<uint16_t>(this->tcp_len_ - used);
  if (this->tcp_len_ != 0) {
    std::memmove(this->tcp_buf_, this->tcp_buf_ + used, this->tcp_len_);
  }
}

void MbapLink::bad_mbap_(uart::UARTComponent *uart) {
  const size_t size = mbap_announced_size(this->tcp_buf_, this->tcp_len_);
  if (size != 0) {
    log_throttled(this->drop_bad_ms_, LOG_STR("Invalid MBAP protocol id, frame skipped"));
    if (size <= this->tcp_len_) {
      this->consume_(size);
      return;
    }
    this->skip_left_ = static_cast<uint16_t>(size - this->tcp_len_);
    this->tcp_len_ = 0;
    return;
  }
  // No usable length, so no frame boundary. A client sends a whole request and then waits.
  log_throttled(this->drop_bad_ms_, LOG_STR("Invalid MBAP length, dropped until the stream is quiet"));
  this->tcp_len_ = 0;
  this->resync_ = true;
  this->resync_from_us_ = micros();
  drain_uart(uart);
}

bool MbapLink::skip_or_resync_(uart::UARTComponent *uart) {
  if (this->resync_) {
    if (uart->available() != 0) {
      drain_uart(uart);
      this->resync_from_us_ = micros();
      return false;
    }
    if (micros() - this->resync_from_us_ < RESYNC_QUIET_US) {
      return false;
    }
    this->resync_ = false;
  }
  uint8_t junk[32];
  while (this->skip_left_ != 0 && uart->available() != 0) {
    size_t n = std::min({static_cast<size_t>(this->skip_left_), uart->available(), sizeof(junk)});
    if (!uart->read_array(junk, n)) {
      return false;
    }
    this->skip_left_ = static_cast<uint16_t>(this->skip_left_ - n);
  }
  return this->skip_left_ == 0;
}

void MbapLink::flush_held_(uart::UARTComponent *uart) {
  if (this->held_len_ < 4) {
    return;
  }
  if (!uart->is_connected()) {
    log_throttled(this->drop_other_ms_, LOG_STR("Not connected, dropped the Modbus frame"));
    this->held_len_ = 0;
    this->tx_sent_ = 0;
    if (this->role_ == Role::ROLE_SERVER) {
      this->txn_pending_ = false;
    }
    return;
  }
  uint16_t txn;
  if (this->role_ == Role::ROLE_SERVER) {
    if (!this->txn_pending_) {
      log_throttled(this->drop_other_ms_, LOG_STR("Reply without a request, dropped"));
      this->held_len_ = 0;
      return;
    }
    txn = this->txn_;
  } else {
    txn = this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
  }
  size_t n = write_mbap(this->tx_, sizeof(this->tx_), txn, this->held_[0], this->held_ + 1, this->held_len_ - 3);
  if (n == 0) {
    log_throttled(this->drop_other_ms_, LOG_STR("Cannot encode the Modbus frame, dropped"));
    this->held_len_ = 0;
    this->tx_sent_ = 0;
    if (this->role_ == Role::ROLE_SERVER) {
      this->txn_pending_ = false;
    }
    return;
  }
  // A short queue takes the MBAP in pieces, the rest is sent from pump(). held_ and the id
  // do not change until the last piece, so encoding again gives the same bytes.
  const size_t chunk = std::min(n - this->tx_sent_, uart->available_for_write());
  if (chunk == 0) {
    return;
  }
  uart->write_array(this->tx_ + this->tx_sent_, chunk);
  this->tx_sent_ = static_cast<uint16_t>(this->tx_sent_ + chunk);
  if (this->tx_sent_ < n) {
    return;
  }
  this->tx_sent_ = 0;
  uart::UARTFlushResult sent = uart->flush();
  // TIMEOUT means the bytes are still queued on a link that is up. They leave later.
  // FAILED drops the frame. The id is not committed, so a late response cannot match it.
  if (sent == uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED) {
    log_throttled(this->drop_other_ms_, LOG_STR("Send failed"));
    this->held_len_ = 0;
    if (this->role_ == Role::ROLE_SERVER) {
      this->txn_pending_ = false;
    }
    return;
  }
  this->held_len_ = 0;
  if (this->role_ == Role::ROLE_SERVER) {
    this->txn_pending_ = false;
  } else {
    // Address 0 is not answered, but it still consumes a transaction id.
    this->txn_ = txn;
    this->unit_ = this->held_[0];
    this->txn_pending_ = true;
  }
}

void MbapLink::pump(uart::UARTComponent *uart) {
  bool up = uart->is_connected();
  if (!up) {
    if (this->link_was_up_ &&
        (this->txn_pending_ || this->tcp_len_ != 0 || this->rtu_len_ != 0 || this->held_len_ != 0)) {
      log_throttled(this->drop_other_ms_, LOG_STR("Link down, dropped the Modbus frame in flight"));
    }
    this->reset_();
    return;
  }
  this->link_was_up_ = true;
  // A held frame, or the rest of one, is sent here.
  this->flush_held_(uart);
  if (!this->skip_or_resync_(uart)) {
    return;
  }
  size_t room = sizeof(this->tcp_buf_) - this->tcp_len_;
  if (room == 0) {
    Mbap frame;
    size_t used = 0;
    // A full buffer that does not start with a whole frame holds a bad header.
    if (take_mbap(this->tcp_buf_, this->tcp_len_, &frame, &used) != MbapTake::MBAP_TAKE_FRAME) {
      this->bad_mbap_(uart);
      return;
    }
  } else if (uart->available() != 0) {
    size_t n = std::min(room, uart->available());
    if (!uart->read_array(this->tcp_buf_ + this->tcp_len_, n)) {
      log_throttled(this->drop_other_ms_, LOG_STR("Read failed"));
      return;
    }
    this->tcp_len_ += static_cast<uint16_t>(n);
  }
  while (this->rtu_len_ == 0 && this->tcp_len_ != 0 && !this->resync_ && this->skip_left_ == 0) {
    if (!this->parse_(uart)) {
      break;
    }
  }
}

bool MbapLink::parse_(uart::UARTComponent *uart) {
  if (this->rtu_len_ != 0 || this->tcp_len_ == 0) {
    return false;
  }
  Mbap frame;
  size_t used = 0;
  switch (take_mbap(this->tcp_buf_, this->tcp_len_, &frame, &used)) {
    case MbapTake::MBAP_TAKE_NEED_MORE:
      return false;
    case MbapTake::MBAP_TAKE_BAD:
      this->bad_mbap_(uart);
      return this->tcp_len_ != 0;
    case MbapTake::MBAP_TAKE_FRAME:
      break;
  }
  if (this->role_ == Role::ROLE_SERVER) {
    // The hub has not taken the last request, or its reply is partly sent. Leave this one in the TCP buffer.
    if (this->rtu_len_ != 0 || this->tx_sent_ != 0) {
      return false;
    }
    if (this->txn_pending_) {
      if (this->held_len_ != 0) {
        log_throttled(this->drop_other_ms_, LOG_STR("Unanswered request replaced"));
        this->held_len_ = 0;
      }
      this->txn_pending_ = false;
    }
    // Address 0 is a broadcast. Nothing answers it.
    if (frame.unit != 0) {
      this->txn_ = frame.txn;
      this->txn_pending_ = true;
    }
  } else if (!this->txn_pending_ || frame.txn != this->txn_) {
    this->note_txn_(frame.txn);
    this->consume_(used);
    return true;
  }

  size_t body = frame.pdu_len + 1;
  bool delivered = false;
  if (body + 2 > sizeof(this->rtu_)) {
    log_throttled(this->drop_other_ms_, LOG_STR("RTU frame too long, dropped"));
  } else {
    this->rtu_[0] = this->role_ == Role::ROLE_SERVER ? frame.unit : this->unit_;
    std::memcpy(this->rtu_ + 1, frame.pdu, frame.pdu_len);
    append_rtu_crc(this->rtu_, body);
    this->rtu_len_ = static_cast<uint16_t>(body + 2);
    delivered = true;
  }
  if (this->role_ != Role::ROLE_SERVER) {
    // Cleared only once the hub can read the response. A dropped frame keeps the id.
    if (delivered) {
      this->txn_pending_ = false;
    }
  } else if (!delivered) {
    this->txn_pending_ = false;
  }
  this->consume_(used);
  // A frame is waiting for the hub. A dropped one leaves the slot free for the next.
  return this->rtu_len_ == 0;
}

size_t MbapLink::take_rtu(uint8_t *dst, size_t cap) {
  if (this->rtu_len_ == 0 || cap < this->rtu_len_) {
    return 0;
  }
  std::memcpy(dst, this->rtu_, this->rtu_len_);
  size_t n = this->rtu_len_;
  this->rtu_len_ = 0;
  return n;
}

void MbapLink::send_rtu(uart::UARTComponent *uart, const uint8_t *rtu, size_t len) {
  if (len < 4 || len > sizeof(this->held_)) {
    log_throttled(this->drop_other_ms_, LOG_STR("Cannot encode the Modbus frame, dropped"));
    return;
  }
  // The new request is already waiting, so this write answers the previous one.
  if (this->role_ == Role::ROLE_SERVER && this->rtu_len_ != 0) {
    log_throttled(this->drop_other_ms_, LOG_STR("Reply to the previous request, dropped"));
    return;
  }
  if (this->role_ == Role::ROLE_SERVER && !this->txn_pending_) {
    log_throttled(this->drop_other_ms_, LOG_STR("Reply without a request, dropped"));
    return;
  }
  // Part of the held frame is on the wire. Another frame in its place would break the stream.
  if (this->tx_sent_ != 0) {
    log_throttled(this->drop_other_ms_, LOG_STR("Still sending, dropped the new Modbus frame"));
    return;
  }
  if (this->held_len_ != 0) {
    log_throttled(this->drop_other_ms_, LOG_STR("Dropping a held Modbus frame"));
    this->held_len_ = 0;
  }
  std::memcpy(this->held_, rtu, len);
  this->held_len_ = static_cast<uint16_t>(len);
  this->flush_held_(uart);
}

}  // namespace esphome::modbus

#endif  // USE_MODBUS_TCP
