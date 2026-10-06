#include "modbus_gateway.h"

#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::modbus_gateway {

static const char *const TAG = "modbus_gateway";

static constexpr uint32_t BAD_LOG_INTERVAL_MS = 5000;

// At most one warning per interval and stamp. A zero stamp means never logged, so the first one after boot is shown.
static bool log_due(uint32_t *stamp) {
  const uint32_t now = App.get_loop_component_start_time();
  if (*stamp != 0 && now - *stamp < BAD_LOG_INTERVAL_MS) {
    return false;
  }
  *stamp = now == 0 ? 1 : now;
  return true;
}
// The Modbus client hub's default. A broadcast is never answered. The pause
// lets the device finish the write before the next frame.
static constexpr uint32_t BROADCAST_TURNAROUND_MS = 600;

// Same character size as the Modbus hub: start bit, data bits, parity and stop bits.
// A UART that never set them is 8N1.
static uint8_t bits_per_char(uart::UARTComponent *uart) {
  uint8_t data_bits = 8;
  uint8_t stop_bits = 1;
  if (uart != nullptr) {
    if (uart->get_data_bits() != 0) {
      data_bits = uart->get_data_bits();
    }
    if (uart->get_stop_bits() != 0) {
      stop_bits = uart->get_stop_bits();
    }
  }
  bool parity = uart != nullptr && uart->get_parity() != uart::UART_CONFIG_PARITY_NONE;
  return static_cast<uint8_t>(1 + data_bits + (parity ? 1 : 0) + stop_bits);
}

// ceil(3.5 character times). Above 19200 baud the spec fixes the gap at
// 1.75 ms. Millis cannot see that, and 1 ms can close a frame too early.
static uint32_t gap_ms(uint32_t baud, uint8_t bits) {
  if (baud == 0 || baud > 19200) {
    return 2;
  }
  return std::max<uint32_t>(1, (uint32_t(bits) * 3500 + baud - 1) / baud);
}

// Time to send one frame of the largest size. After a timeout the bus stays closed
// until it was quiet this long, so a late response is dropped.
static uint32_t frame_ms(uint32_t baud, uint8_t bits) {
  if (baud == 0) {
    baud = 9600;
  }
  return (uint32_t(MAX_FRAME) * bits * 1000 + baud - 1) / baud;
}

// The address and the function must belong to the request that is on the bus, and a read
// must carry the byte count of the quantity it asked for. An exception sets the high bit of the function.
// key is the request's unit, function, start and quantity.
static bool response_matches(const uint8_t *key, const uint8_t *response, uint16_t response_len) {
  if (response_len < 3 || response[0] != key[0]) {
    return false;
  }
  uint8_t function = key[1];
  if (response[1] == static_cast<uint8_t>(function | 0x80)) {
    return true;
  }
  if (response[1] != function) {
    return false;
  }
  if (!modbus::helpers::is_function_code_read_only(function)) {
    return true;
  }
  uint16_t quantity = encode_uint16(key[4], key[5]);
  bool bits = function == modbus::FunctionCode::READ_COILS || function == modbus::FunctionCode::READ_DISCRETE_INPUTS;
  size_t expected = bits ? modbus::packed_bit_bytes(quantity) : size_t(quantity) * 2;
  return response[2] == expected;
}

struct Inspect {
  uint16_t len{0};
  bool drop_all{false};
  bool bad_crc{false};
};

static uint16_t crc_frame(const uint8_t *data, uint16_t size) {
  uint16_t crc = crc16(data, 4);
  if (crc == 0) {
    return 4;
  }
  for (uint16_t at = 4; at < size; at++) {
    crc = crc16(&data[at], 1, crc);
    if (crc == 0) {
      return at + 1;
    }
  }
  return 0;
}

static Inspect inspect(const uint8_t *buf, uint16_t size, bool request, bool silent) {
  Inspect out;
  if (size < 4) {
    out.drop_all = size > 0 && silent;
    return out;
  }
  bool unknown = modbus::helpers::is_function_code_unknown_length(buf[1]);
  uint16_t need = unknown ? crc_frame(buf, size)
                          : (request ? modbus::helpers::client_frame_length(buf, size)
                                     : modbus::helpers::server_frame_length(buf, size));
  if (need == 0 || size < need) {
    if (silent || size >= MAX_FRAME) {
      out.bad_crc = size >= 4;
      out.drop_all = true;
    }
    return out;
  }
  if (!unknown && crc16(buf, need) != 0) {
    out.bad_crc = true;
    return out;
  }
  out.len = need;
  return out;
}

static void consume(uint8_t *buf, uint16_t *len, uint16_t used) {
  uint16_t rest = *len - used;
  std::memmove(buf, buf + used, rest);
  *len = rest;
}

void GatewayUart::write_array(const uint8_t *data, size_t len) { this->gateway_->port_write(this->index_, data, len); }

size_t GatewayUart::available_for_write() { return this->gateway_->port_room(this->index_); }

// The bytes leave through the bus only in the port's turn. Report them as still queued until then.
uart::UARTFlushResult GatewayUart::flush() {
  return this->gateway_->port_empty(this->index_) ? uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS
                                                  : uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
}

void ModbusGateway::port_write(uint8_t index, const uint8_t *data, size_t len) {
  Port &port = this->ports_[index];
  // A newer request replaces the one that waits for the bus.
  if (len > static_cast<size_t>(MAX_FRAME - port.len) && port.pending_len != 0) {
    consume(port.data, &port.len, port.pending_len);
    port.pending_len = 0;
  }
  if (len > static_cast<size_t>(MAX_FRAME - port.len)) {
    if (log_due(&this->last_response_log_ms_)) {
      ESP_LOGW(TAG, "Write dropped, the buffer holds one frame");
    }
    return;
  }
  std::memcpy(port.data + port.len, data, len);
  port.len += static_cast<uint16_t>(len);
  port.last_ms = App.get_loop_component_start_time();
}

void ModbusGateway::log_bad_(bool crc) {
  if (!log_due(&this->last_bad_log_ms_)) {
    return;
  }
  if (crc) {
    ESP_LOGW(TAG, "Dropping a frame with a bad CRC");
    return;
  }
  ESP_LOGW(TAG, "Dropping an incomplete frame");
}

void ModbusGateway::log_mismatch_() {
  if (log_due(&this->last_mismatch_log_ms_)) {
    ESP_LOGW(TAG, "Response ignored, it does not match the request");
  }
}

bool ModbusGateway::write_frame_(uart::UARTComponent *dest, const uint8_t *data, uint16_t len) {
  // A partial write would open a gap on the wire longer than one frame.
  size_t room = dest->available_for_write();
  if (room != SIZE_MAX && room < len) {
    return false;
  }
  dest->write_array(data, len);
  return true;
}

bool ModbusGateway::deliver_(uint8_t index, const uint8_t *data, uint16_t len) {
  Port &port = this->ports_[index];
  if (port.local) {
    if (!static_cast<GatewayUart *>(port.uart)->inject_rx(data, len)) {
      this->log_dropped_(false, len);
      return false;
    }
    return true;
  }
  if (!this->write_frame_(port.uart, data, len)) {
    this->log_dropped_(true, len);
    return false;
  }
  return true;
}

void ModbusGateway::log_dropped_(bool on_uart, uint16_t len) {
  if (!log_due(&this->last_response_log_ms_)) {
    return;
  }
  if (!on_uart) {
    ESP_LOGW(TAG, "Response dropped, the buffer holds one frame");
    return;
  }
  ESP_LOGW(TAG, "Response dropped, the UART cannot take %u bytes", static_cast<unsigned>(len));
}

// The bytes behind the waiting request are parsed. A complete request there replaces the waiting one:
// a client that polls again before its turn keeps only the latest request.
void ModbusGateway::take_requests_(uint8_t index, uint32_t now) {
  Port &port = this->ports_[index];
  uint32_t baud = port.uart->get_baud_rate() != 0 ? port.uart->get_baud_rate() : 9600;
  bool silent = now - port.last_ms >= gap_ms(baud, bits_per_char(port.uart));
  while (port.len > port.pending_len) {
    uint8_t *rest = port.data + port.pending_len;
    uint16_t rest_len = port.len - port.pending_len;
    Inspect found = inspect(rest, rest_len, true, silent);
    if (found.len != 0) {
      consume(port.data, &port.len, port.pending_len);
      port.pending_len = found.len;
      continue;
    }
    if (found.bad_crc && !found.drop_all) {
      this->log_bad_(true);
      consume(rest, &rest_len, 1);
      port.len = port.pending_len + rest_len;
      continue;
    }
    if (found.drop_all) {
      this->log_bad_(found.bad_crc);
      port.len = port.pending_len;
      return;
    }
    if (port.len < MAX_FRAME || port.pending_len == 0) {
      return;
    }
    // Full: the newer request needs the room of the waiting one.
    consume(port.data, &port.len, port.pending_len);
    port.pending_len = 0;
  }
}

void ModbusGateway::read_port_(uint8_t index, uint32_t now) {
  Port &port = this->ports_[index];
  if (!port.local) {
    // When the buffer is full the bytes stay in the port's UART.
    size_t n = std::min(port.uart->available(), static_cast<size_t>(MAX_FRAME - port.len));
    if (n != 0 && port.uart->read_array(port.data + port.len, n)) {
      port.len += static_cast<uint16_t>(n);
      port.last_ms = now;
    }
  }
  this->take_requests_(index, now);
}

void ModbusGateway::read_bus_(uint32_t now) {
  size_t n = std::min(this->parent_->available(), static_cast<size_t>(MAX_FRAME - this->bus_len_));
  if (n != 0 && this->parent_->read_array(this->bus_ + this->bus_len_, n)) {
    this->bus_len_ += static_cast<uint16_t>(n);
    this->bus_last_ms_ = now;
  }
  uint32_t gap = gap_ms(this->parent_->get_baud_rate(), bits_per_char(this->parent_));
  while (this->bus_len_ > 0 && this->active_ >= 0) {
    bool silent = now - this->bus_last_ms_ >= gap;
    Inspect found = inspect(this->bus_, this->bus_len_, false, silent);
    if (found.len == 0) {
      if (found.bad_crc && !found.drop_all) {
        this->log_bad_(true);
        consume(this->bus_, &this->bus_len_, 1);
        continue;
      }
      if (found.drop_all) {
        this->log_bad_(found.bad_crc);
        this->bus_len_ = 0;
      }
      return;
    }
    if (!response_matches(this->request_key_, this->bus_, found.len)) {
      this->log_mismatch_();
      consume(this->bus_, &this->bus_len_, found.len);
      continue;
    }
    uint8_t index = static_cast<uint8_t>(this->active_);
    // A full port keeps the frame. Dropping it here would end the transaction
    // and the next client could be given this answer.
    if (!this->deliver_(index, this->bus_, found.len)) {
      return;
    }
    consume(this->bus_, &this->bus_len_, found.len);
    this->active_ = -1;
  }
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
    if (!this->write_frame_(this->parent_, port.data, port.pending_len)) {
      return false;
    }
    // The first six bytes are always in the buffer. Bytes past a shorter request are not read.
    std::memcpy(this->request_key_, port.data, sizeof(this->request_key_));
    consume(port.data, &port.len, port.pending_len);
    port.pending_len = 0;
    this->active_ = static_cast<int8_t>(index);
    this->sent_ms_ = now;
    this->bus_len_ = 0;
    // Address 0 is a broadcast. Nothing may answer. loop() holds the bus
    // for the turnaround and drops anything that arrives meanwhile.
    this->awaiting_ = this->request_key_[0] != 0;
    this->next_port_ = static_cast<uint8_t>((index + 1) % this->port_count_);
    return true;
  }
  return false;
}

// Nothing is expected while no request is open. It is read into bus_ and dropped.
void ModbusGateway::drain_bus_(uint32_t now) {
  size_t n = std::min(this->parent_->available(), sizeof(this->bus_));
  if (n != 0 && this->parent_->read_array(this->bus_, n)) {
    this->bus_last_ms_ = now;
  }
  this->bus_len_ = 0;
}

void ModbusGateway::loop() { this->run_(App.get_loop_component_start_time()); }

void ModbusGateway::run_(uint32_t now) {
  if (this->port_count_ == 0 || this->parent_ == nullptr) {
    return;
  }
  for (uint8_t i = 0; i < this->port_count_; i++) {
    this->read_port_(i, now);
  }
  if (this->active_ >= 0) {
    if (!this->awaiting_) {
      if (now - this->sent_ms_ >= BROADCAST_TURNAROUND_MS) {
        this->active_ = -1;
      }
    } else {
      this->read_bus_(now);
      if (this->active_ >= 0 && now - this->sent_ms_ >= this->response_timeout_ms_) {
        if (log_due(&this->last_timeout_log_ms_)) {
          ESP_LOGW(TAG, "No response from the bus");
        }
        this->active_ = -1;
        this->bus_len_ = 0;
        this->quarantine_ = true;
        this->bus_last_ms_ = now;
      }
    }
  }
  if (this->active_ < 0) {
    this->drain_bus_(now);
  }
  if (this->quarantine_) {
    if (now - this->bus_last_ms_ < frame_ms(this->parent_->get_baud_rate(), bits_per_char(this->parent_))) {
      return;
    }
    this->quarantine_ = false;
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
                "  Ports: %u",
                this->response_timeout_ms_, this->port_count_);
}

}  // namespace esphome::modbus_gateway
