#include "modbus_gateway.h"

#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <new>
#include <utility>

namespace esphome::modbus_gateway {

static const char *const TAG = "modbus_gateway";

static constexpr uint32_t BAD_LOG_INTERVAL_MS = 5000;
// The Modbus client hub's default. A broadcast is never answered. The pause
// lets the slave finish the write before the next frame.
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

// The address and the function must belong to the request that is on the bus.
// An exception response sets the high bit of that function code.
static bool response_matches(const uint8_t *request, uint16_t request_len, const uint8_t *response,
                             uint16_t response_len) {
  if (request_len < 2 || response_len < 2 || response[0] != request[0]) {
    return false;
  }
  uint8_t function = request[1];
  return response[1] == function || response[1] == static_cast<uint8_t>(function | 0x80);
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

void GatewayUart::write_array(const uint8_t *data, size_t len) {
  size_t room = MAX_FRAME - this->tx_len_;
  if (len > room) {
    if (!this->trunc_logged_) {
      this->trunc_logged_ = true;
      ESP_LOGW(TAG, "Write dropped, the buffer holds one frame");
    }
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

void ModbusGateway::log_bad_(uint32_t now, bool crc) {
  if (now - this->last_bad_log_ms_ < BAD_LOG_INTERVAL_MS) {
    return;
  }
  this->last_bad_log_ms_ = now;
  if (crc) {
    ESP_LOGW(TAG, "Dropping a frame with a bad CRC");
    return;
  }
  ESP_LOGW(TAG, "Dropping an incomplete frame");
}

void ModbusGateway::log_mismatch_(uint32_t now) {
  if (now - this->last_mismatch_log_ms_ < BAD_LOG_INTERVAL_MS) {
    return;
  }
  this->last_mismatch_log_ms_ = now;
  ESP_LOGW(TAG, "Response ignored, it does not match the request");
}

bool ModbusGateway::write_frame_(uart::UARTComponent *dest, const uint8_t *data, uint16_t len) {
  if (dest == nullptr || len == 0) {
    return false;
  }
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
  if (port.local != nullptr) {
    if (!port.local->push_rx(data, len)) {
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
  uint32_t now = millis();
  if (now - this->last_response_log_ms_ < BAD_LOG_INTERVAL_MS) {
    return;
  }
  this->last_response_log_ms_ = now;
  if (!on_uart) {
    ESP_LOGW(TAG, "Response dropped, the buffer holds one frame");
    return;
  }
  ESP_LOGW(TAG, "Response dropped, the UART cannot take %u bytes", static_cast<unsigned>(len));
}

bool ModbusGateway::fresh_(const CacheSlot &slot, uint32_t now) const {
  return slot.len != 0 && now - slot.ms < this->cache_time_ms_;
}

bool ModbusGateway::prepare_cache_() {
  if (this->slots_ != nullptr) {
    return true;
  }
  static_assert(alignof(CacheSlot) <= alignof(std::max_align_t), "cache slots need aligned storage");
  CacheSlot *slots = RAMAllocator<CacheSlot>().allocate(this->cache_limit_);
  if (slots == nullptr) {
    if (!this->cache_alloc_failed_) {
      this->cache_alloc_failed_ = true;
      ESP_LOGW(TAG, "Cache disabled, no memory for %u entries", this->cache_limit_);
    }
    return false;
  }
  this->slots_ = slots;
  this->cache_cap_ = this->cache_limit_;
  return true;
}

void ModbusGateway::release_cache_() {
  if (this->slots_ == nullptr) {
    return;
  }
  for (uint16_t i = 0; i < this->cache_size_; i++) {
    this->slots_[i].~CacheSlot();
  }
  RAMAllocator<CacheSlot>().deallocate(this->slots_, this->cache_cap_);
  this->slots_ = nullptr;
  this->cache_size_ = 0;
  this->cache_cap_ = 0;
}

ModbusGateway::~ModbusGateway() { this->release_cache_(); }

ModbusGateway::CacheSlot &ModbusGateway::add_slot_() {
  CacheSlot *slot = new (this->slots_ + this->cache_size_) CacheSlot();
  this->cache_size_++;
  return *slot;
}

void ModbusGateway::pop_slot_() {
  this->cache_size_--;
  this->slots_[this->cache_size_].~CacheSlot();
}

bool ModbusGateway::fill_slot_(CacheSlot &slot, const uint8_t *request, const uint8_t *response, uint16_t response_len,
                               uint32_t now) {
  if (slot.cap < response_len) {
    auto buffer = RAMAllocator<uint8_t>().make_unique_array_for_overwrite(response_len);
    if (buffer == nullptr) {
      if (!this->cache_alloc_failed_) {
        this->cache_alloc_failed_ = true;
        ESP_LOGW(TAG, "Cache skipped a response, no memory for %u bytes", response_len);
      }
      return false;
    }
    slot.data = std::move(buffer);
    slot.cap = response_len;
  }
  std::memcpy(slot.data.get(), response, response_len);
  std::memcpy(slot.key, request, sizeof(slot.key));
  slot.len = response_len;
  slot.ms = now;
  return true;
}

void ModbusGateway::move_front_(size_t index) {
  if (index == 0) {
    return;
  }
  CacheSlot moved = std::move(this->slots_[index]);
  for (size_t i = index; i > 0; i--) {
    this->slots_[i] = std::move(this->slots_[i - 1]);
  }
  this->slots_[0] = std::move(moved);
}

void ModbusGateway::note_drop_(uint32_t now) {
  this->cache_drops_++;
  if (now - this->last_drop_log_ms_ < BAD_LOG_INTERVAL_MS) {
    return;
  }
  ESP_LOGD(TAG, "Cache dropped %u entries, the table holds %u", this->cache_drops_, this->cache_size_);
  this->cache_drops_ = 0;
  this->last_drop_log_ms_ = now;
}

ModbusGateway::CacheTake ModbusGateway::serve_from_cache_(uint8_t index, const uint8_t *data, uint16_t len,
                                                          uint32_t now) {
  if (this->cache_limit_ == 0 || !this->ports_[index].use_cache || this->cache_time_ms_ == 0 || len < 6 ||
      !modbus::helpers::is_function_code_read_only(data[1])) {
    return CacheTake::MISS;
  }
  for (uint16_t i = 0; i < this->cache_size_; i++) {
    CacheSlot &slot = this->slots_[i];
    if (!this->fresh_(slot, now) || std::memcmp(slot.key, data, sizeof(slot.key)) != 0) {
      continue;
    }
    if (!this->deliver_(index, slot.data.get(), slot.len)) {
      return CacheTake::BLOCKED;
    }
    this->move_front_(i);
    return CacheTake::SERVED;
  }
  return CacheTake::MISS;
}

void ModbusGateway::store_cache_(const uint8_t *request, uint16_t request_len, const uint8_t *response,
                                 uint16_t response_len, uint32_t now) {
  if (this->cache_limit_ == 0 || this->cache_time_ms_ == 0 || request_len < 6 || response_len < 5 ||
      response_len > MAX_FRAME || !modbus::helpers::is_function_code_read_only(request[1]) ||
      response[1] != request[1]) {
    return;
  }
  if (!this->prepare_cache_()) {
    return;
  }
  for (uint16_t i = 0; i < this->cache_size_; i++) {
    if (this->slots_[i].len == 0 || std::memcmp(this->slots_[i].key, request, sizeof(this->slots_[i].key)) != 0) {
      continue;
    }
    if (!this->fill_slot_(this->slots_[i], request, response, response_len, now)) {
      return;
    }
    this->move_front_(i);
    return;
  }
  for (uint16_t i = 0; i < this->cache_size_; i++) {
    if (this->fresh_(this->slots_[i], now)) {
      continue;
    }
    if (!this->fill_slot_(this->slots_[i], request, response, response_len, now)) {
      return;
    }
    this->move_front_(i);
    return;
  }
  if (this->cache_size_ < this->cache_cap_) {
    CacheSlot &slot = this->add_slot_();
    if (!this->fill_slot_(slot, request, response, response_len, now)) {
      this->pop_slot_();
      return;
    }
    this->move_front_(this->cache_size_ - 1);
    return;
  }
  uint16_t last = this->cache_size_ - 1;
  bool live = this->fresh_(this->slots_[last], now);
  if (!this->fill_slot_(this->slots_[last], request, response, response_len, now)) {
    return;
  }
  if (live) {
    this->note_drop_(now);
  }
  this->move_front_(last);
}

void ModbusGateway::clear_cache_() {
  for (uint16_t i = 0; i < this->cache_size_; i++) {
    this->slots_[i].len = 0;
  }
}

void ModbusGateway::take_requests_(uint8_t index, uint32_t now) {
  Port &port = this->ports_[index];
  uart::UARTComponent *end = this->endpoint_(index);
  uint32_t baud = end != nullptr && end->get_baud_rate() != 0 ? end->get_baud_rate() : 9600;
  uint32_t gap = gap_ms(baud, bits_per_char(end));
  while (port.len > 0) {
    bool silent = now - port.last_ms >= gap;
    Inspect found = inspect(port.data, port.len, true, silent);
    if (found.len == 0) {
      if (found.bad_crc && !found.drop_all && port.len > 0) {
        this->log_bad_(now, true);
        consume(port.data, &port.len, 1);
        continue;
      }
      if (found.drop_all) {
        this->log_bad_(now, found.bad_crc);
        port.len = 0;
      }
      return;
    }
    // A master that polls again before its turn keeps only the latest request.
    std::memcpy(port.pending, port.data, found.len);
    port.pending_len = found.len;
    consume(port.data, &port.len, found.len);
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
  while (this->bus_len_<MAX_FRAME &&this->parent_->available()> 0) {
    size_t n = std::min(this->parent_->available(), sizeof(tmp));
    n = std::min(n, static_cast<size_t>(MAX_FRAME - this->bus_len_));
    if (!this->parent_->read_array(tmp, n)) {
      break;
    }
    std::memcpy(this->bus_ + this->bus_len_, tmp, n);
    this->bus_len_ += static_cast<uint16_t>(n);
    this->bus_last_ms_ = now;
  }
  uint32_t gap = gap_ms(this->parent_->get_baud_rate(), bits_per_char(this->parent_));
  while (this->bus_len_ > 0 && this->active_ >= 0) {
    bool silent = now - this->bus_last_ms_ >= gap;
    Inspect found = inspect(this->bus_, this->bus_len_, false, silent);
    if (found.len == 0) {
      if (found.bad_crc && !found.drop_all) {
        this->log_bad_(now, true);
        consume(this->bus_, &this->bus_len_, 1);
        continue;
      }
      if (found.drop_all) {
        this->log_bad_(now, found.bad_crc);
        this->bus_len_ = 0;
      }
      return;
    }
    if (!response_matches(this->request_, this->request_len_, this->bus_, found.len)) {
      this->log_mismatch_(now);
      consume(this->bus_, &this->bus_len_, found.len);
      continue;
    }
    uint8_t index = static_cast<uint8_t>(this->active_);
    // A full port keeps the frame. Dropping it here would end the transaction
    // and the next master could be given this answer.
    if (!this->deliver_(index, this->bus_, found.len)) {
      return;
    }
    this->store_cache_(this->request_, this->request_len_, this->bus_, found.len, now);
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
    switch (this->serve_from_cache_(index, port.pending, port.pending_len, now)) {
      case CacheTake::SERVED:
        port.pending_len = 0;
        this->next_port_ = static_cast<uint8_t>((index + 1) % this->port_count_);
        return true;
      case CacheTake::BLOCKED:
        continue;
      case CacheTake::MISS:
        break;
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
    // Address 0 is a broadcast. Slaves must not answer. loop() holds the bus
    // for the turnaround and drops anything that arrives meanwhile.
    this->awaiting_ = this->request_[0] != 0;
    this->next_port_ = static_cast<uint8_t>((index + 1) % this->port_count_);
    if (!modbus::helpers::is_function_code_read_only(this->request_[1])) {
      this->clear_cache_();
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
      if (now - this->sent_ms_ >= BROADCAST_TURNAROUND_MS) {
        this->active_ = -1;
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
  ESP_LOGCONFIG(TAG, "Modbus Gateway:");
  ESP_LOGCONFIG(TAG, "  Response timeout: %" PRIu32 " ms", this->response_timeout_ms_);
  if (this->cache_limit_ == 0 || this->cache_time_ms_ == 0) {
    ESP_LOGCONFIG(TAG, "  Cache: off");
  } else {
    ESP_LOGCONFIG(TAG, "  Cache time: %" PRIu32 " ms", this->cache_time_ms_);
    ESP_LOGCONFIG(TAG, "  Cache entries: %u", this->cache_limit_);
  }
  ESP_LOGCONFIG(TAG, "  Ports: %u", this->port_count_);
  for (uint8_t i = 0; i < this->port_count_; i++) {
    ESP_LOGCONFIG(TAG, "  Port %u: cache %s", i,
                  this->ports_[i].use_cache ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"));
  }
}

}  // namespace esphome::modbus_gateway
