#include "modbus_gateway.h"

#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <new>

namespace esphome::modbus_gateway {

static const char *const TAG = "modbus_gateway";

static constexpr uint32_t US_PER_SEC = 1000000;
static constexpr uint32_t BAD_LOG_INTERVAL_MS = 5000;
// The Modbus client hub's default. A broadcast is never answered. The pause
// lets the device finish the write before the next frame.
static constexpr uint32_t BROADCAST_TURNAROUND_US = 600000;
// A frame ends after 3.5 quiet characters (here in half characters), and after at least 1750 us above 19200 baud,
// as in Modbus RTU and the modbus hub.
static constexpr uint32_t FRAME_GAP_HALF_CHARS = 7;
static constexpr uint32_t MIN_FRAME_GAP_US = 1750;
// The modbus hub's wait for the rest of a frame from a UART that hands over bytes in chunks.
static constexpr uint32_t CHUNKED_FRAME_GAP_US = 50000;
// One character at 9600 baud 8N1, for a bus that reports no baud rate.
static constexpr uint32_t DEFAULT_CHAR_US = 1042;
// Loop passes come every 16 ms. Closer to a deadline than this, or while a FIFO is fed, pass at full speed.
// A UART that takes nothing for this long, and for two characters, has stalled.
static constexpr uint32_t FAST_WAIT_US = 20000;

// At most one warning per interval and stamp. A zero stamp means never logged, so the first one after boot is shown.
static bool log_due(uint32_t *stamp) {
  const uint32_t now = App.get_loop_component_start_time();
  if (*stamp != 0 && now - *stamp < BAD_LOG_INTERVAL_MS) {
    return false;
  }
  *stamp = now == 0 ? 1 : now;
  return true;
}

// Same character size as the Modbus hub: start bit, data bits, parity and stop bits.
// A UART that never set them is 8N1.
static uint32_t bits_per_char(uart::UARTComponent *uart) {
  const uint32_t data_bits = uart->get_data_bits() != 0 ? uart->get_data_bits() : 8;
  const uint32_t stop_bits = uart->get_stop_bits() != 0 ? uart->get_stop_bits() : 1;
  return 1 + data_bits + (uart->get_parity() == uart::UART_CONFIG_PARITY_NONE ? 0 : 1) + stop_bits;
}

// Wire time of one character in us, rounded up; 0 when the UART has no baud rate.
static uint32_t char_time_us(uart::UARTComponent *uart) {
  const uint32_t baud = uart->get_baud_rate();
  return baud == 0 ? 0 : (bits_per_char(uart) * US_PER_SEC + baud - 1) / baud;
}

static uint32_t gap_after(uint32_t char_us) {
  return std::max(MIN_FRAME_GAP_US, (char_us * FRAME_GAP_HALF_CHARS + 1) / 2);
}

// ESP-IDF moves received bytes on once rx_full_threshold are in the FIFO, or after rx_timeout quiet characters.
// After a full batch the rest of a frame can take that long to show up, as the modbus hub allows. 0: no batches.
static uint32_t batch_after(uart::UARTComponent *uart, uint32_t char_us, uint32_t gap_us) {
  const size_t threshold = uart->get_rx_full_threshold();
  if (threshold == uart::UARTComponent::RX_FULL_THRESHOLD_UNSET) {
    return 0;
  }
  return std::max(gap_us, static_cast<uint32_t>(threshold + uart->get_rx_timeout()) * char_us);
}

static uint32_t remaining(uint32_t elapsed, uint32_t total) { return elapsed < total ? total - elapsed : 0; }

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
  // A newer request replaces the one that waits for the bus, unless that one is going out already.
  if (len > static_cast<size_t>(MAX_FRAME - port.len) && port.pending_len != 0 && !this->sending_(index)) {
    consume(port.data, &port.len, port.pending_len);
    port.pending_len = 0;
  }
  if (len > static_cast<size_t>(MAX_FRAME - port.len)) {
    this->log_drop_(LOG_STR("Port buffer full"), len);
    return;
  }
  std::memcpy(port.data + port.len, data, len);
  port.len += static_cast<uint16_t>(len);
}

void ModbusGateway::log_bad_(bool crc) {
  if (log_due(&this->last_bad_log_ms_)) {
    ESP_LOGW(TAG, "Dropping %s",
             crc ? LOG_STR_LITERAL("a frame with a bad CRC") : LOG_STR_LITERAL("an incomplete frame"));
  }
}

void ModbusGateway::log_mismatch_() {
  if (log_due(&this->last_mismatch_log_ms_)) {
    ESP_LOGW(TAG, "Response ignored, it does not match the request");
  }
}

void ModbusGateway::log_drop_(const LogString *why, size_t len) {
  if (log_due(&this->last_drop_log_ms_)) {
    ESP_LOGW(TAG, "%s, dropped %zu bytes", LOG_STR_ARG(why), len);
  }
}

void ModbusGateway::setup() {
  const uint32_t char_us = char_time_us(this->parent_);
  if (this->bus_clocked_ && char_us != 0) {
    this->bus_char_us_ = char_us;
    this->bus_tx_gap_us_ = gap_after(char_us);
    this->bus_rx_gap_us_ = this->bus_tx_gap_us_;
    this->bus_batch_us_ = batch_after(this->parent_, char_us, this->bus_rx_gap_us_);
  } else {
    this->bus_rx_gap_us_ = CHUNKED_FRAME_GAP_US;
  }
  // After a timeout the bus stays closed until it was quiet for one frame of the largest size, so a late
  // response is dropped.
  this->bus_closed_us_ = std::max(this->bus_rx_gap_us_, MAX_FRAME * (char_us != 0 ? char_us : DEFAULT_CHAR_US));
  for (uint8_t i = 0; i < this->port_count_; i++) {
    Port &port = this->ports_[i];
    // A hub on a local port writes whole frames; they need no gap.
    if (port.local) {
      continue;
    }
    const uint32_t port_char_us = char_time_us(port.uart);
    if (!port.clocked || port_char_us == 0) {
      port.gap_us = CHUNKED_FRAME_GAP_US;
      continue;
    }
    port.gap_us = gap_after(port_char_us);
    port.batch_us = batch_after(port.uart, port_char_us, port.gap_us);
  }
  if (this->cache_count_ == 0) {
    return;
  }
  // The whole cache in one block, allocated once. RAMAllocator takes PSRAM when the device has it.
  this->slots_ = RAMAllocator<CacheSlot>().allocate(this->cache_count_);
  if (this->slots_ == nullptr) {
    ESP_LOGE(TAG, "No memory for %u cache entries", this->cache_count_);
    this->cache_count_ = 0;
    return;
  }
  for (uint16_t i = 0; i < this->cache_count_; i++) {
    new (&this->slots_[i]) CacheSlot();
  }
}

// The bytes behind the waiting request are parsed. A complete request there replaces the waiting one:
// a client that polls again before its turn keeps only the latest request.
void ModbusGateway::take_requests_(uint8_t index, uint32_t now) {
  // The request in front goes out to the bus. What the client sent after it waits behind it.
  if (this->sending_(index)) {
    return;
  }
  Port &port = this->ports_[index];
  bool silent = now - port.last_us >= (port.batched ? port.batch_us : port.gap_us);
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
      port.last_us = now;
      port.batched = port.batch_us != 0 && n >= port.uart->get_rx_full_threshold();
    }
  }
  this->take_requests_(index, now);
}

// Writes what dest takes now. A FIFO gets what fits and the rest on the next passes, without a pause. A UART that
// cannot report its room (SIZE_MAX) gets all of it in one write, as the modbus hub writes a frame.
ModbusGateway::Write ModbusGateway::write_more_(uart::UARTComponent *dest, const uint8_t *data, uint32_t now) {
  if (!dest->is_connected()) {
    return Write::WRITE_DOWN;
  }
  size_t n = std::min(static_cast<size_t>(this->out_len_ - this->out_sent_), dest->available_for_write());
  if (n == 0) {
    // A FIFO frees room within a character.
    return now - this->progress_us_ < std::max(FAST_WAIT_US, 2 * char_time_us(dest)) ? Write::WRITE_MORE
                                                                                     : Write::WRITE_STALLED;
  }
  dest->write_array(data + this->out_sent_, n);
  if (dest == this->parent_) {
    // The line sends these bytes after whatever it still has: from now, or from the end of that.
    if (now - this->tx_start_us_ >= this->tx_busy_us_) {
      this->tx_start_us_ = now;
      this->tx_busy_us_ = 0;
    }
    this->tx_busy_us_ += static_cast<uint32_t>(n) * this->bus_char_us_;
  }
  this->out_sent_ += static_cast<uint16_t>(n);
  this->progress_us_ = now;
  return this->out_sent_ == this->out_len_ ? Write::WRITE_DONE : Write::WRITE_MORE;
}

void ModbusGateway::start_next_(uint32_t now, uint32_t now_ms) {
  for (uint8_t n = 0; n < this->port_count_; n++) {
    uint8_t index = static_cast<uint8_t>((this->next_port_ + n) % this->port_count_);
    Port &port = this->ports_[index];
    if (port.pending_len == 0) {
      continue;
    }
    // The first six bytes are always in the buffer. Bytes past a shorter request are not read.
    std::memcpy(this->request_key_, port.data, sizeof(this->request_key_));
    this->active_ = index;
    this->next_port_ = static_cast<uint8_t>((index + 1) % this->port_count_);
    if (this->serve_cached_(index, now, now_ms)) {
      return;
    }
    // A write, or any request that is not a read, may change what is stored for its unit.
    if (!modbus::helpers::is_function_code_read_only(this->request_key_[1])) {
      this->clear_cache_(this->request_key_[0]);
    }
    this->out_len_ = port.pending_len;
    this->out_sent_ = 0;
    this->progress_us_ = now;
    this->phase_ = Phase::PHASE_SENDING;
    this->send_request_(now);
    return;
  }
}

void ModbusGateway::send_request_(uint32_t now) {
  Port &port = this->ports_[this->active_];
  const Write result = this->write_more_(this->parent_, port.data, now);
  if (result == Write::WRITE_MORE) {
    return;
  }
  if (result != Write::WRITE_DONE) {
    this->log_drop_(result == Write::WRITE_DOWN ? LOG_STR("Bus not connected") : LOG_STR("Bus stalled"),
                    this->out_len_ - this->out_sent_);
  }
  // What the client sent after the request moves up.
  consume(port.data, &port.len, port.pending_len);
  port.pending_len = 0;
  if (result != Write::WRITE_DONE) {
    this->phase_ = Phase::PHASE_IDLE;
    return;
  }
  // The response timeout starts when the request has left the wire.
  this->sent_us_ = this->tx_start_us_;
  this->bus_len_ = 0;
  if (this->request_key_[0] == 0) {
    // Address 0 is a broadcast. Nothing may answer. The bus is held for the turnaround
    // and anything that arrives meanwhile is dropped.
    this->phase_ = Phase::PHASE_TURNAROUND;
    this->wait_us_ = this->tx_busy_us_ + BROADCAST_TURNAROUND_US;
    return;
  }
  this->phase_ = Phase::PHASE_AWAITING;
  this->wait_us_ = this->tx_busy_us_ + this->response_timeout_us_;
}

void ModbusGateway::read_bus_(uint32_t now, uint32_t now_ms) {
  size_t n = std::min(this->parent_->available(), static_cast<size_t>(MAX_FRAME - this->bus_len_));
  if (n != 0 && this->parent_->read_array(this->bus_ + this->bus_len_, n)) {
    this->bus_len_ += static_cast<uint16_t>(n);
    this->bus_last_us_ = now;
    this->bus_batched_ = this->bus_batch_us_ != 0 && n >= this->parent_->get_rx_full_threshold();
  }
  bool silent = now - this->bus_last_us_ >= (this->bus_batched_ ? this->bus_batch_us_ : this->bus_rx_gap_us_);
  while (this->bus_len_ > 0) {
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
    this->store_cache_(found.len, now_ms);
    this->take_response_(found.len, now);
    return;
  }
}

// The bus answered. A port that cannot take the response loses it, and the bus is free again.
void ModbusGateway::take_response_(uint16_t len, uint32_t now) {
  Port &port = this->ports_[this->active_];
  if (port.local) {
    if (!static_cast<GatewayUart *>(port.uart)->inject_rx(this->bus_, len)) {
      this->log_drop_(LOG_STR("Port buffer full"), len);
    }
    this->phase_ = Phase::PHASE_IDLE;
    return;
  }
  this->out_len_ = len;
  this->out_sent_ = 0;
  this->progress_us_ = now;
  this->phase_ = Phase::PHASE_DELIVERING;
  this->send_response_(now);
}

void ModbusGateway::send_response_(uint32_t now) {
  const Write result = this->write_more_(this->ports_[this->active_].uart, this->bus_, now);
  if (result == Write::WRITE_MORE) {
    return;
  }
  if (result != Write::WRITE_DONE) {
    this->log_drop_(result == Write::WRITE_DOWN ? LOG_STR("Port not connected") : LOG_STR("Port stalled"),
                    this->out_len_ - this->out_sent_);
  }
  this->phase_ = Phase::PHASE_IDLE;
}

// A fresh stored response to the same read goes to the port instead of the request going to the bus.
bool ModbusGateway::serve_cached_(uint8_t index, uint32_t now, uint32_t now_ms) {
  Port &port = this->ports_[index];
  if (!port.use_cache || !modbus::helpers::is_function_code_read_only(this->request_key_[1])) {
    return false;
  }
  for (uint16_t i = 0; i < this->cache_count_; i++) {
    CacheSlot &slot = this->slots_[i];
    if (slot.len == 0 || now_ms - slot.stored_ms >= this->cache_time_ms_ ||
        std::memcmp(slot.key, this->request_key_, sizeof(slot.key)) != 0) {
      continue;
    }
    slot.used = ++this->cache_seq_;
    consume(port.data, &port.len, port.pending_len);
    port.pending_len = 0;
    std::memcpy(this->bus_, slot.data, slot.len);
    this->take_response_(slot.len, now);
    return true;
  }
  return false;
}

// The response to a read goes into the entry of the same read, else into an empty or expired one, else into the
// one used longest ago.
void ModbusGateway::store_cache_(uint16_t len, uint32_t now_ms) {
  if (this->cache_count_ == 0 || this->bus_[1] != this->request_key_[1] || len > MAX_READ_RESPONSE ||
      !modbus::helpers::is_function_code_read_only(this->request_key_[1])) {
    return;
  }
  CacheSlot *pick = nullptr;
  bool pick_free = false;
  for (uint16_t i = 0; i < this->cache_count_; i++) {
    CacheSlot &slot = this->slots_[i];
    if (slot.len != 0 && std::memcmp(slot.key, this->request_key_, sizeof(slot.key)) == 0) {
      pick = &slot;
      pick_free = true;
      break;
    }
    const bool free = slot.len == 0 || now_ms - slot.stored_ms >= this->cache_time_ms_;
    if (pick == nullptr || (free && !pick_free) ||
        (!free && !pick_free && static_cast<int32_t>(slot.used - pick->used) < 0)) {
      pick = &slot;
      pick_free = free;
    }
  }
  if (!pick_free) {
    this->cache_evicted_++;
    if (log_due(&this->last_evict_log_ms_)) {
      ESP_LOGD(TAG, "Replaced %u fresh stored reads, cache_entries %u may be too few", this->cache_evicted_,
               this->cache_count_);
      this->cache_evicted_ = 0;
    }
  }
  std::memcpy(pick->data, this->bus_, len);
  std::memcpy(pick->key, this->request_key_, sizeof(pick->key));
  pick->len = len;
  pick->stored_ms = now_ms;
  pick->used = ++this->cache_seq_;
}

// Unit 0 drops every stored read.
void ModbusGateway::clear_cache_(uint8_t unit) {
  for (uint16_t i = 0; i < this->cache_count_; i++) {
    if (unit == 0 || this->slots_[i].key[0] == unit) {
      this->slots_[i].len = 0;
    }
  }
}

// Nothing is expected while no request is open. It is read into bus_ and dropped.
void ModbusGateway::drain_bus_(uint32_t now) {
  size_t n = std::min(this->parent_->available(), sizeof(this->bus_));
  if (n != 0 && this->parent_->read_array(this->bus_, n)) {
    this->bus_last_us_ = now;
  }
  this->bus_len_ = 0;
}

// Time until the bus was quiet long enough for a request to start: a gap after the last byte either way, and one
// frame of the largest size after a timeout.
uint32_t ModbusGateway::bus_wait_(uint32_t now) const {
  return std::max(remaining(now - this->bus_last_us_, this->quarantine_ ? this->bus_closed_us_ : this->bus_tx_gap_us_),
                  remaining(now - this->tx_start_us_, this->tx_busy_us_ + this->bus_tx_gap_us_));
}

// Full-speed loop passes only while a FIFO is fed or a deadline is closer than a normal pass.
void ModbusGateway::pace_(uint32_t now) {
  uint32_t wait = FAST_WAIT_US;
  switch (this->phase_) {
    case Phase::PHASE_SENDING:
    case Phase::PHASE_DELIVERING:
      wait = 0;
      break;
    case Phase::PHASE_AWAITING:
    case Phase::PHASE_TURNAROUND:
      wait = remaining(now - this->sent_us_, this->wait_us_);
      break;
    case Phase::PHASE_IDLE:
      for (uint8_t i = 0; i < this->port_count_; i++) {
        if (this->ports_[i].pending_len != 0) {
          wait = this->bus_wait_(now);
          break;
        }
      }
      break;
  }
  if (wait < FAST_WAIT_US) {
    this->fast_.start();
  } else {
    this->fast_.stop();
  }
}

// micros(): above 19200 baud the gap before a request is 1.75 ms.
void ModbusGateway::loop() { this->run_(micros(), App.get_loop_component_start_time()); }

void ModbusGateway::run_(uint32_t now, uint32_t now_ms) {
  for (uint8_t i = 0; i < this->port_count_; i++) {
    this->read_port_(i, now);
  }
  switch (this->phase_) {
    case Phase::PHASE_SENDING:
      this->send_request_(now);
      break;
    case Phase::PHASE_AWAITING:
      this->read_bus_(now, now_ms);
      if (this->phase_ == Phase::PHASE_AWAITING && now - this->sent_us_ >= this->wait_us_) {
        if (log_due(&this->last_timeout_log_ms_)) {
          ESP_LOGW(TAG, "No response from the bus");
        }
        this->phase_ = Phase::PHASE_IDLE;
        this->quarantine_ = true;
        this->bus_last_us_ = now;
        // A stored read of a unit that went silent is not served any more.
        this->clear_cache_(this->request_key_[0]);
      }
      break;
    case Phase::PHASE_TURNAROUND:
      this->drain_bus_(now);
      if (now - this->sent_us_ >= this->wait_us_) {
        this->phase_ = Phase::PHASE_IDLE;
      }
      break;
    case Phase::PHASE_DELIVERING:
      this->send_response_(now);
      break;
    case Phase::PHASE_IDLE:
      break;
  }
  if (this->phase_ == Phase::PHASE_IDLE) {
    this->drain_bus_(now);
    if (this->bus_wait_(now) == 0) {
      this->quarantine_ = false;
      this->start_next_(now, now_ms);
    }
  }
  this->pace_(now);
}

void ModbusGateway::dump_config() {
  // One letter per port: y may be answered from the cache, n always uses the bus.
  char cache[MODBUS_GATEWAY_PORT_COUNT + 1]{};
  for (uint8_t i = 0; i < this->port_count_; i++) {
    cache[i] = this->ports_[i].use_cache ? 'y' : 'n';
  }
  ESP_LOGCONFIG(TAG,
                "Modbus Gateway:\n"
                "  Response timeout: %" PRIu32 " ms\n"
                "  Frame gap: %" PRIu32 " us\n"
                "  Cache: %u entries of %u bytes, %" PRIu32 " ms\n"
                "  Ports: %u, cache per port: %s",
                this->response_timeout_us_ / 1000, this->bus_rx_gap_us_, this->cache_count_,
                static_cast<unsigned>(sizeof(CacheSlot)), this->cache_time_ms_, this->port_count_, cache);
}

}  // namespace esphome::modbus_gateway
