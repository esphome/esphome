#include "modbus_gateway.h"

#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::modbus_gateway {

static const char *const TAG = "modbus_gateway";

static constexpr uint32_t BITS_PER_CHAR = 11;
static constexpr uint32_t BAD_LOG_INTERVAL_MS = 5000;

static uint32_t gap_ms(uint32_t baud) {
  if (baud == 0) {
    return 2;
  }
  // 3.5 characters. One millisecond is the shortest silence millis() can see.
  uint32_t us = (35 * BITS_PER_CHAR * 100) / baud;
  return std::max<uint32_t>(1, (us + 999) / 1000);
}

static uint32_t wire_ms(uint32_t baud, uint16_t len) {
  if (baud == 0) {
    return gap_ms(0);
  }
  return std::max<uint32_t>(1, (uint32_t(len) * BITS_PER_CHAR * 1000) / baud) + gap_ms(baud);
}

static bool is_read(uint8_t function_code) {
  uint8_t code = function_code & 0x7F;
  return code >= 0x01 && code <= 0x04;
}

static uint16_t expected_len(const uint8_t *data, uint16_t size, bool request) {
  if (request) {
    return modbus::helpers::client_frame_length(data, size);
  }
  return modbus::helpers::server_frame_length(data, size);
}

static uint16_t crc_frame(const uint8_t *data, uint16_t size) {
  if (size < 4) {
    return 0;
  }
  uint16_t crc = crc16(data, 4);
  if (crc == 0) {
    return 4;
  }
  for (uint16_t len = 4; len < size; len++) {
    crc = crc16(&data[len], 1, crc);
    if (crc == 0) {
      return len + 1;
    }
  }
  return 0;
}

void GatewayUart::write_array(const uint8_t *data, size_t len) {
  size_t room = MAX_FRAME - this->tx_len_;
  if (len > room) {
    len = room;
  }
  if (len == 0) {
    return;
  }
  std::memcpy(this->tx_ + this->tx_len_, data, len);
  this->tx_len_ += static_cast<uint16_t>(len);
}

bool GatewayUart::peek_byte(uint8_t *data) {
  if (this->rx_len_ == 0) {
    return false;
  }
  *data = this->rx_[0];
  return true;
}

bool GatewayUart::read_array(uint8_t *data, size_t len) {
  if (len > this->rx_len_) {
    return false;
  }
  std::memcpy(data, this->rx_, len);
  this->rx_len_ -= static_cast<uint16_t>(len);
  if (this->rx_len_ > 0) {
    std::memmove(this->rx_, this->rx_ + len, this->rx_len_);
  }
  return true;
}

size_t GatewayUart::take_tx(uint8_t *dest, size_t cap) {
  size_t n = std::min(cap, static_cast<size_t>(this->tx_len_));
  if (n == 0) {
    return 0;
  }
  std::memcpy(dest, this->tx_, n);
  this->tx_len_ -= static_cast<uint16_t>(n);
  if (this->tx_len_ > 0) {
    std::memmove(this->tx_, this->tx_ + n, this->tx_len_);
  }
  return n;
}

bool GatewayUart::push_rx(const uint8_t *data, size_t len) {
  if (len > MAX_FRAME - this->rx_len_) {
    return false;
  }
  std::memcpy(this->rx_ + this->rx_len_, data, len);
  this->rx_len_ += static_cast<uint16_t>(len);
  return true;
}

uart::UARTComponent *ModbusGateway::endpoint_(uint8_t index) {
  Port &port = this->ports_[index];
  if (port.local != nullptr) {
    return port.local;
  }
  return port.uart;
}

void ModbusGateway::log_bad_(uint32_t now) {
  if (now - this->last_bad_log_ms_ < BAD_LOG_INTERVAL_MS) {
    return;
  }
  this->last_bad_log_ms_ = now;
  ESP_LOGW(TAG, "Dropping a frame with a bad CRC");
}

bool ModbusGateway::write_frame_(uart::UARTComponent *dest, const uint8_t *data, uint16_t len) {
  if (dest == nullptr || len == 0) {
    return false;
  }
  // A partial write would put a gap on the wire longer than 3.5 characters.
  size_t room = dest->available_for_write();
  if (room != SIZE_MAX && room < len) {
    return false;
  }
  dest->write_array(data, len);
  return true;
}

void ModbusGateway::deliver_(uint8_t index, const uint8_t *data, uint16_t len) {
  Port &port = this->ports_[index];
  if (port.local != nullptr) {
    if (!port.local->push_rx(data, len)) {
      ESP_LOGW(TAG, "Response dropped, the UART buffer is full");
    }
    return;
  }
  if (!this->write_frame_(port.uart, data, len)) {
    ESP_LOGW(TAG, "Response dropped, the UART cannot take %u bytes", len);
  }
}

bool ModbusGateway::serve_from_cache_(uint8_t index, const uint8_t *data, uint16_t len, uint32_t now) {
  if (!this->cache_valid_ || this->cache_time_ms_ == 0 || len < 6 || !is_read(data[1])) {
    return false;
  }
  if (now - this->cache_ms_ >= this->cache_time_ms_) {
    return false;
  }
  if (std::memcmp(this->cache_key_, data, sizeof(this->cache_key_)) != 0) {
    return false;
  }
  this->deliver_(index, this->cache_, this->cache_len_);
  return true;
}

void ModbusGateway::store_cache_(const uint8_t *request, uint16_t request_len, const uint8_t *response,
                                 uint16_t response_len, uint32_t now) {
  if (this->cache_time_ms_ == 0 || request_len < 6 || !is_read(request[1]) || response_len > MAX_FRAME) {
    return;
  }
  std::memcpy(this->cache_key_, request, sizeof(this->cache_key_));
  std::memcpy(this->cache_, response, response_len);
  this->cache_len_ = response_len;
  this->cache_ms_ = now;
  this->cache_valid_ = true;
}

void ModbusGateway::take_requests_(uint8_t index, uint32_t now) {
  Port &port = this->ports_[index];
  uart::UARTComponent *end = this->endpoint_(index);
  uint32_t baud = end != nullptr && end->get_baud_rate() != 0 ? end->get_baud_rate() : 9600;
  uint32_t gap = gap_ms(baud);
  while (port.len >= 4) {
    bool unknown = modbus::helpers::is_function_code_unknown_length(port.data[1]);
    uint16_t need = unknown ? crc_frame(port.data, port.len) : expected_len(port.data, port.len, true);
    bool silent = now - port.last_ms >= gap;
    if (need == 0 || port.len < need) {
      if (!silent && port.len < MAX_FRAME) {
        return;
      }
      this->log_bad_(now);
      port.len = 0;
      return;
    }
    if (!unknown && crc16(port.data, need) != 0) {
      this->log_bad_(now);
      std::memmove(port.data, port.data + 1, --port.len);
      continue;
    }
    // A master that polls again before its turn keeps only the latest request.
    std::memcpy(port.pending, port.data, need);
    port.pending_len = need;
    uint16_t rest = port.len - need;
    std::memmove(port.data, port.data + need, rest);
    port.len = rest;
  }
}

void ModbusGateway::read_port_(uint8_t index, uint32_t now) {
  Port &port = this->ports_[index];
  uart::UARTComponent *end = this->endpoint_(index);
  if (end == nullptr) {
    return;
  }
  uint8_t tmp[64];
  while (port.len < MAX_FRAME) {
    size_t n = 0;
    if (port.local != nullptr) {
      n = port.local->take_tx(tmp, std::min(sizeof(tmp), static_cast<size_t>(MAX_FRAME - port.len)));
    } else {
      size_t waiting = end->available();
      if (waiting == 0) {
        break;
      }
      n = std::min(waiting, sizeof(tmp));
      n = std::min(n, static_cast<size_t>(MAX_FRAME - port.len));
      if (!end->read_array(tmp, n)) {
        break;
      }
    }
    if (n == 0) {
      break;
    }
    std::memcpy(port.data + port.len, tmp, n);
    port.len += static_cast<uint16_t>(n);
    port.last_ms = now;
  }
  this->take_requests_(index, now);
}

void ModbusGateway::read_bus_(uint32_t now) {
  uint8_t tmp[64];
  while (this->bus_len_ < MAX_FRAME && this->parent_->available() > 0) {
    size_t n = std::min(this->parent_->available(), sizeof(tmp));
    n = std::min(n, static_cast<size_t>(MAX_FRAME - this->bus_len_));
    if (!this->parent_->read_array(tmp, n)) {
      break;
    }
    std::memcpy(this->bus_ + this->bus_len_, tmp, n);
    this->bus_len_ += static_cast<uint16_t>(n);
    this->bus_last_ms_ = now;
  }
  if (this->bus_len_ < 4) {
    return;
  }
  bool unknown = modbus::helpers::is_function_code_unknown_length(this->bus_[1]);
  uint16_t need = unknown ? crc_frame(this->bus_, this->bus_len_) : expected_len(this->bus_, this->bus_len_, false);
  uint32_t gap = gap_ms(this->parent_->get_baud_rate());
  bool silent = now - this->bus_last_ms_ >= gap;
  if (need == 0 || this->bus_len_ < need) {
    if (!silent && this->bus_len_ < MAX_FRAME) {
      return;
    }
    this->log_bad_(now);
    this->bus_len_ = 0;
    return;
  }
  if (!unknown && crc16(this->bus_, need) != 0) {
    if (!silent) {
      return;
    }
    this->log_bad_(now);
    this->bus_len_ = 0;
    return;
  }
  uint8_t index = static_cast<uint8_t>(this->active_);
  this->deliver_(index, this->bus_, need);
  this->store_cache_(this->request_, this->request_len_, this->bus_, need, now);
  uint16_t rest = this->bus_len_ - need;
  std::memmove(this->bus_, this->bus_ + need, rest);
  this->bus_len_ = rest;
  this->active_ = -1;
}

bool ModbusGateway::start_next_(uint32_t now) {
  if (this->active_ >= 0) {
    return false;
  }
  for (uint8_t n = 0; n < this->port_count_; n++) {
    uint8_t index = static_cast<uint8_t>((this->next_port_ + n) % this->port_count_);
    Port &port = this->ports_[index];
    if (port.pending_len == 0) {
      continue;
    }
    if (this->serve_from_cache_(index, port.pending, port.pending_len, now)) {
      port.pending_len = 0;
      this->next_port_ = static_cast<uint8_t>((index + 1) % this->port_count_);
      return true;
    }
    if (!this->write_frame_(this->parent_, port.pending, port.pending_len)) {
      return false;
    }
    std::memcpy(this->request_, port.pending, port.pending_len);
    this->request_len_ = port.pending_len;
    port.pending_len = 0;
    this->active_ = static_cast<int8_t>(index);
    this->sent_ms_ = now;
    this->bus_len_ = 0;
    // Address 0 is a broadcast. There is no response to route.
    this->awaiting_ = this->request_[0] != 0;
    this->next_port_ = static_cast<uint8_t>((index + 1) % this->port_count_);
    if (!is_read(this->request_[1])) {
      this->cache_valid_ = false;
    }
    return true;
  }
  return false;
}

void ModbusGateway::loop() {
  if (this->port_count_ == 0 || this->parent_ == nullptr) {
    return;
  }
  uint32_t now = millis();
  for (uint8_t i = 0; i < this->port_count_; i++) {
    this->read_port_(i, now);
  }
  if (this->active_ >= 0) {
    if (!this->awaiting_) {
      if (now - this->sent_ms_ >= wire_ms(this->parent_->get_baud_rate(), this->request_len_)) {
        this->active_ = -1;
      }
    } else if (now - this->sent_ms_ < wire_ms(this->parent_->get_baud_rate(), this->request_len_)) {
      // The request is still shifting out. Bytes read here are the echo, not the response.
      this->bus_len_ = 0;
      uint8_t echo[32];
      while (this->parent_->available() > 0) {
        size_t n = std::min(this->parent_->available(), sizeof(echo));
        if (!this->parent_->read_array(echo, n)) {
          break;
        }
      }
    } else {
      this->read_bus_(now);
      if (this->active_ >= 0 && now - this->sent_ms_ >= this->response_timeout_ms_) {
        if (now - this->last_timeout_log_ms_ >= BAD_LOG_INTERVAL_MS) {
          this->last_timeout_log_ms_ = now;
          ESP_LOGW(TAG, "No response from the bus");
        }
        this->active_ = -1;
        this->bus_len_ = 0;
      }
    }
  }
  if (this->active_ < 0) {
    uint8_t junk[32];
    while (this->parent_->available() > 0) {
      size_t n = std::min(this->parent_->available(), sizeof(junk));
      if (!this->parent_->read_array(junk, n)) {
        break;
      }
    }
  }
  for (uint8_t n = 0; n < this->port_count_ && this->active_ < 0; n++) {
    if (!this->start_next_(now)) {
      break;
    }
  }
}

void ModbusGateway::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Modbus Gateway:\n"
                "  Response timeout: %" PRIu32 " ms\n"
                "  Cache time: %" PRIu32 " ms\n"
                "  Ports: %u",
                this->response_timeout_ms_, this->cache_time_ms_, this->port_count_);
}

}  // namespace esphome::modbus_gateway
