#ifdef USE_MODBUS_TCP_UART

#include "modbus_tcp_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::modbus_tcp {

static const char *const TAG = "modbus_tcp";

// Bytes per 16 ms loop pass at 10 bits per byte: baud / 10 / 62.5.
static constexpr uint32_t BAUD_PACE_DIVISOR = 625;
static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;
// Modbus exception: gateway target device failed to respond.
static constexpr uint8_t EXCEPTION_GATEWAY_TARGET = 0x0B;

static void note_drop(uint32_t &last_ms, const LogString *message) {
  uint32_t now = App.get_loop_component_start_time();
  if (last_ms != 0 && now - last_ms < DROP_LOG_INTERVAL_MS) {
    return;
  }
  // A zero stamp would look like "never logged" on the next pass.
  last_ms = now == 0 ? 1 : now;
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(message));
}

void ModbusTcpUart::setup() {
  this->link_.begin(TAG);
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.begin(TAG);
#endif
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void ModbusTcpUart::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Modbus TCP:\n"
                "  %s: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms\n"
                "  Timeout: %" PRIu32 "ms\n"
                "  Send Wait Time: %" PRIu32 "ms",
                this->server_ ? LOG_STR_LITERAL("Listen") : LOG_STR_LITERAL("Host"),
                this->server_ ? LOG_STR_LITERAL("*") : this->link_.host(), this->link_.port(),
                this->link_.reconnect_interval(), this->timeout_ms_, this->send_wait_ms_);
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.dump_config();
#endif
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void ModbusTcpUart::on_shutdown() {
  this->link_.close();
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.close();
#endif
}

void ModbusTcpUart::note_io_() {
  uint32_t now = App.get_loop_component_start_time();
  this->last_io_ms_ = now == 0 ? 1 : now;
}

void ModbusTcpUart::note_uart_() {
  uint32_t now = micros();
  this->last_uart_us_ = now == 0 ? 1 : now;
}

void ModbusTcpUart::sync_link_() {
  bool up = this->link_.connected();
  this->link_was_up_ = up;
  if (up) {
    // The driver kept whatever arrived while the link was down.
    this->discard_uart_();
    this->note_io_();
  }
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void ModbusTcpUart::discard_uart_() {
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

bool ModbusTcpUart::maintain_link_() {
#ifdef USE_SOCKET_TCP_LISTENER
  if (this->server_) {
    // Accept on the next pass, after the drop edge.
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
    return false;
  }
  this->check_timeout_();
  return this->link_.connected();
}

void ModbusTcpUart::check_timeout_() {
  if (this->timeout_ms_ == 0 || !this->link_was_up_) {
    return;
  }
  if (App.get_loop_component_start_time() - this->last_io_ms_ < this->timeout_ms_) {
    return;
  }
  ESP_LOGW(TAG, "Timeout, closing");
  this->link_.close();
  this->link_.note_attempt();
}

void ModbusTcpUart::loop() {
  if (!this->maintain_link_()) {
    this->tcp_len_ = 0;
    this->uart_len_ = 0;
    this->rtu_tx_len_ = 0;
    this->rtu_tx_off_ = 0;
    this->wait_uart_ = false;
    this->wait_tcp_ = false;
    this->arm_wait_uart_ = false;
    return;
  }
  this->pump_modbus_();
  this->link_.flush_tx();
}

uint32_t ModbusTcpUart::frame_gap_us_() const {
  uint32_t baud = std::max<uint32_t>(1, this->parent_->get_baud_rate());
  // The spec's fixed 1750 us gap applies above 19200 baud.
  if (baud > 19200) {
    return 1750;
  }
  uint8_t data = this->parent_->get_data_bits() != 0 ? this->parent_->get_data_bits() : 8;
  uint8_t stop = this->parent_->get_stop_bits() != 0 ? this->parent_->get_stop_bits() : 1;
  uint32_t bits = 1u + data + (this->parent_->get_parity() == uart::UART_CONFIG_PARITY_NONE ? 0u : 1u) + stop;
  return (bits * 3500000u) / baud + 1;
}

void ModbusTcpUart::read_tcp_buf_() {
  size_t room = sizeof(this->tcp_buf_) - this->tcp_len_;
  if (room == 0 || !this->link_.ready()) {
    return;
  }
  ssize_t count = this->link_.read(this->tcp_buf_ + this->tcp_len_, std::min(room, READ_CHUNK));
  if (count > 0) {
    this->tcp_len_ += static_cast<uint16_t>(count);
    this->note_io_();
  }
}

MbapTake ModbusTcpUart::take_tcp_(uint8_t *pdu, size_t cap, size_t *pdu_len, uint16_t *txn, uint8_t *unit) {
  Mbap frame;
  size_t used = 0;
  MbapTake got = take_mbap(this->tcp_buf_, this->tcp_len_, &frame, &used);
  if (got == MbapTake::NEED_MORE) {
    return MbapTake::NEED_MORE;
  }
  if (got != MbapTake::FRAME || frame.pdu_len > cap) {
    if (used == 0 || used > this->tcp_len_) {
      used = 1;
    }
    std::memmove(this->tcp_buf_, this->tcp_buf_ + used, this->tcp_len_ - used);
    this->tcp_len_ -= static_cast<uint16_t>(used);
    return MbapTake::BAD;
  }
  // frame.pdu points into tcp_buf_. Copy it before the tail slides.
  std::memcpy(pdu, frame.pdu, frame.pdu_len);
  *pdu_len = frame.pdu_len;
  *txn = frame.txn;
  *unit = frame.unit;
  std::memmove(this->tcp_buf_, this->tcp_buf_ + used, this->tcp_len_ - used);
  this->tcp_len_ -= static_cast<uint16_t>(used);
  return MbapTake::FRAME;
}

void ModbusTcpUart::pull_uart_buf_() {
  size_t room = sizeof(this->uart_buf_) - this->uart_len_;
  size_t want = std::min(room, this->available());
  if (want == 0) {
    return;
  }
  if (this->read_array(this->uart_buf_ + this->uart_len_, want)) {
    this->uart_len_ += static_cast<uint16_t>(want);
    this->note_uart_();
    this->note_io_();
  }
}

bool ModbusTcpUart::take_rtu_(uint8_t *pdu, size_t *pdu_len, uint8_t *unit) {
  if (this->uart_len_ < 4 || micros() - this->last_uart_us_ < this->frame_gap_us_()) {
    return false;
  }
  if (!rtu_crc_ok(this->uart_buf_, this->uart_len_)) {
    note_drop(this->last_drop_log_ms_, LOG_STR("RTU CRC mismatch"));
    this->uart_len_ = 0;
    return false;
  }
  *unit = this->uart_buf_[0];
  *pdu_len = this->uart_len_ - 3;
  std::memcpy(pdu, this->uart_buf_ + 1, *pdu_len);
  this->uart_len_ = 0;
  return true;
}

bool ModbusTcpUart::drain_rtu_() {
  if (this->rtu_tx_off_ >= this->rtu_tx_len_) {
    return true;
  }
  size_t room = this->parent_->available_for_write();
  if (room == SIZE_MAX) {
    // Capacity unknown on this platform; pace to one loop pass of UART time
    // so a blocking write stays short.
    room = std::max<size_t>(1, this->parent_->get_baud_rate() / BAUD_PACE_DIVISOR);
  }
  size_t left = this->rtu_tx_len_ - this->rtu_tx_off_;
  size_t n = std::min(room, left);
  if (n == 0) {
    return false;
  }
  this->parent_->write_array(this->rtu_tx_ + this->rtu_tx_off_, n);
  this->rtu_tx_off_ += static_cast<uint16_t>(n);
  if (this->rtu_tx_off_ < this->rtu_tx_len_) {
    return false;
  }
  this->rtu_tx_len_ = 0;
  this->rtu_tx_off_ = 0;
  return true;
}

bool ModbusTcpUart::write_rtu_(const uint8_t *pdu, size_t pdu_len, uint8_t unit) {
  if (pdu_len + 3 > sizeof(this->rtu_tx_) || this->rtu_tx_len_ != 0) {
    return false;
  }
  this->rtu_tx_[0] = unit;
  std::memcpy(this->rtu_tx_ + 1, pdu, pdu_len);
  size_t len = pdu_len + 1;
  uint16_t crc = crc16(this->rtu_tx_, static_cast<uint16_t>(len));
  this->rtu_tx_[len++] = crc & 0xFF;
  this->rtu_tx_[len++] = crc >> 8;
  this->rtu_tx_len_ = static_cast<uint16_t>(len);
  this->rtu_tx_off_ = 0;
  return this->drain_rtu_();
}

bool ModbusTcpUart::send_mbap_(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
  uint8_t frame[TCP_FRAME_SIZE];
  size_t n = write_mbap(frame, sizeof(frame), txn, unit, pdu, pdu_len);
  if (n == 0) {
    return false;
  }
  if (this->link_.tx_free() < n) {
    note_drop(this->last_drop_log_ms_, LOG_STR("TX buffer full, dropped the Modbus frame"));
    return false;
  }
  this->link_.queue(frame, n);
  this->note_io_();
  return true;
}

void ModbusTcpUart::send_exception_() {
  uint8_t exc[2] = {static_cast<uint8_t>(this->pending_function_ | 0x80), EXCEPTION_GATEWAY_TARGET};
  this->send_mbap_(this->txn_, this->pending_unit_, exc, sizeof(exc));
}

void ModbusTcpUart::pump_modbus_() {
  uint8_t pdu[PDU_MAX];
  size_t pdu_len = 0;
  uint8_t unit = 0;
  uint16_t txn = 0;
  uint32_t now = App.get_loop_component_start_time();
  // The response clock starts once the request has left the driver. Timing it
  // out mid-write would put a partial RTU frame on the wire and then 0x0B.
  if (this->rtu_tx_len_ != 0 && !this->drain_rtu_()) {
    return;
  }
  if (this->arm_wait_uart_) {
    this->arm_wait_uart_ = false;
    this->wait_uart_ = true;
    this->wait_started_ms_ = now == 0 ? 1 : now;
    this->note_uart_();
  }
  if ((this->wait_uart_ || this->wait_tcp_) && now - this->wait_started_ms_ >= this->send_wait_ms_) {
    ESP_LOGW(TAG, "Modbus response timeout");
    if (this->wait_uart_) {
      this->send_exception_();
      // A late reply must not sit in the driver and stick to the next request.
      this->discard_uart_();
    }
    this->uart_len_ = 0;
    this->tcp_len_ = 0;
    this->rtu_tx_len_ = 0;
    this->rtu_tx_off_ = 0;
    this->wait_uart_ = false;
    this->wait_tcp_ = false;
    this->arm_wait_uart_ = false;
    return;
  }
  if (this->wait_uart_) {
    this->pull_uart_buf_();
    if (this->take_rtu_(pdu, &pdu_len, &unit)) {
      this->send_mbap_(this->txn_, unit, pdu, pdu_len);
      this->wait_uart_ = false;
    }
    return;
  }
  if (this->wait_tcp_) {
    this->read_tcp_buf_();
    while (this->tcp_len_ != 0) {
      switch (this->take_tcp_(pdu, sizeof(pdu), &pdu_len, &txn, &unit)) {
        case MbapTake::NEED_MORE:
          return;
        case MbapTake::BAD:
          continue;
        case MbapTake::FRAME:
          break;
      }
      if (txn != this->txn_) {
        uint32_t stamp = App.get_loop_component_start_time();
        if (this->last_drop_log_ms_ == 0 || stamp - this->last_drop_log_ms_ >= DROP_LOG_INTERVAL_MS) {
          this->last_drop_log_ms_ = stamp == 0 ? 1 : stamp;
          ESP_LOGW(TAG, "Dropped transaction %u, expected %u", txn, this->txn_);
        }
        continue;
      }
      this->write_rtu_(pdu, pdu_len, unit);
      this->wait_tcp_ = false;
      return;
    }
    return;
  }
#ifdef USE_SOCKET_TCP_LISTENER
  if (this->server_) {
    this->read_tcp_buf_();
    while (this->tcp_len_ != 0) {
      switch (this->take_tcp_(pdu, sizeof(pdu), &pdu_len, &txn, &unit)) {
        case MbapTake::NEED_MORE:
          return;
        case MbapTake::BAD:
          continue;
        case MbapTake::FRAME:
          break;
      }
      this->discard_uart_();
      this->uart_len_ = 0;
      this->txn_ = txn;
      this->pending_unit_ = unit;
      this->pending_function_ = pdu[0];
      if (!this->write_rtu_(pdu, pdu_len, unit)) {
        this->arm_wait_uart_ = true;
        return;
      }
      this->wait_uart_ = true;
      this->wait_started_ms_ = now == 0 ? 1 : now;
      this->note_uart_();
      return;
    }
    return;
  }
#endif
  this->pull_uart_buf_();
  if (!this->take_rtu_(pdu, &pdu_len, &unit)) {
    return;
  }
  txn = this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
  if (!this->send_mbap_(txn, unit, pdu, pdu_len)) {
    return;
  }
  this->txn_ = txn;
  this->wait_tcp_ = true;
  this->wait_started_ms_ = now == 0 ? 1 : now;
}

}  // namespace esphome::modbus_tcp
#endif
