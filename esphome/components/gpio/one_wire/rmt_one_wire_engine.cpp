#include "rmt_one_wire_engine.h"

#ifdef USE_ONE_WIRE_RMT

#include <algorithm>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::gpio::rmt_one_wire {

ESPHOME_LOG_TAG(TAG, "gpio.one_wire");

using one_wire::Transaction;
using one_wire::TransactionStatus;

static constexpr uint8_t CMD_SKIP_ROM = 0xCC;
static constexpr uint8_t CMD_MATCH_ROM = 0x55;

static constexpr uint32_t NO_RX = 0;
static constexpr uint32_t RX_IDLE_RESET_US = 600;  // above the reset Low, or the capture ends inside the pulse
static constexpr uint32_t SECTION_TIMEOUT_MS = 50;
static constexpr uint32_t BLOCKING_WAIT_MS = 1;

Engine::Engine(Port *port, size_t mem_words, const Timing &timing)
    : port_(port),
      mem_words_(mem_words),
      timing_(timing),
      rx_idle_us_(std::max<uint32_t>(100, timing.slot_us + 20)) {}  // above the longest phase of a section

bool Engine::submit(Transaction *transaction) {
  if (transaction->pending_)
    return false;
  transaction->pending_ = true;
  transaction->next_ = nullptr;
  if (this->head_ == nullptr) {
    this->head_ = transaction;
  } else {
    this->tail_->next_ = transaction;
  }
  this->tail_ = transaction;
  return true;
}

void Engine::poll() {
  if (this->head_ == nullptr)
    return;
  if (this->failed_) {
    this->finish_(TransactionStatus::HARDWARE_ERROR);
    return;
  }
  switch (this->step_) {
    case Step::LINE_UNCHECKED:
      // RX sees no edge on a line that is already Low, so this would end as a timeout much later
      if (this->port_->line_is_high()) {
        this->start_reset_();
      } else {
        this->finish_(TransactionStatus::BUS_LOW);
      }
      break;
    case Step::RESET:
    case Step::WRITE:
    case Step::READ:
      // The completion goes first: a main loop that was blocked for long is no 1-Wire error
      if (this->pending_ == 0) {
        this->section_done_();
      } else if (this->port_->now_ms() - this->since_ms_ >= SECTION_TIMEOUT_MS) {
        this->finish_(this->abort_section_() ? TransactionStatus::TIMEOUT : TransactionStatus::HARDWARE_ERROR);
      }
      break;
  }
}

void Engine::set_failed() {
  this->failed_ = true;
  this->pending_ = 0;
}

bool IRAM_ATTR Engine::on_tx_done() { return this->pending_.fetch_and(~PENDING_TX) == PENDING_TX; }

bool IRAM_ATTR Engine::on_rx_done(size_t num_symbols) {
  if (this->pending_ & PENDING_RX)
    this->rx_count_ = num_symbols;
  return this->pending_.fetch_and(~PENDING_RX) == PENDING_RX;
}

void Engine::start_reset_() {
  const uint64_t address = this->head_->address;
  this->rom_selection_[0] = address == 0 ? CMD_SKIP_ROM : CMD_MATCH_ROM;
  for (size_t i = 0; i < 8; i++)
    this->rom_selection_[i + 1] = static_cast<uint8_t>(address >> (8 * i));
  this->rom_bits_ = address == 0 ? 8 : 72;
  this->write_pos_ = 0;
  this->read_pos_ = 0;

  this->step_ = Step::RESET;
  this->tx_[0] = reset_symbol();
  if (!this->start_section_(1, RX_IDLE_RESET_US))
    this->finish_(TransactionStatus::HARDWARE_ERROR);
}

void Engine::section_done_() {
  const Transaction *transaction = this->head_;
  TransactionStatus status = TransactionStatus::OK;
  if (this->step_ == Step::RESET) {
    status = decode_reset(this->rx_, this->rx_count_);
  } else if (this->step_ == Step::READ &&
             !this->decode_read_(this->read_slots_, transaction->read_data, this->read_pos_ - this->read_slots_)) {
    status = TransactionStatus::CAPTURE_ERROR;
  }
  if (status != TransactionStatus::OK) {
    this->finish_(status);
    return;
  }

  // ROM selection and write_data share one stream of slots, read_data follows in sections of its own
  const size_t write_end = this->rom_bits_ + transaction->write_len * 8u;
  const size_t read_end = transaction->read_len * 8u;
  size_t count = 0;
  uint32_t idle_us = NO_RX;
  if (this->write_pos_ < write_end) {
    this->step_ = Step::WRITE;
    const size_t room = max_tx_slots(this->mem_words_);
    for (; count < room && this->write_pos_ < write_end; count++, this->write_pos_++) {
      const bool in_rom = this->write_pos_ < this->rom_bits_;
      encode_write_slots(in_rom ? this->rom_selection_ : transaction->write_data,
                         in_rom ? this->write_pos_ : this->write_pos_ - this->rom_bits_, 1, this->tx_ + count,
                         this->timing_);
    }
  } else if (this->read_pos_ < read_end) {
    this->step_ = Step::READ;
    count = encode_read_slots(std::min(max_rx_slots(this->mem_words_), read_end - this->read_pos_), this->tx_,
                              this->timing_);
    this->read_slots_ = count;
    this->read_pos_ += count;
    idle_us = this->rx_idle_us_;
  } else {
    this->finish_(TransactionStatus::OK);
    return;
  }
  if (!this->start_section_(count, idle_us))
    this->finish_(TransactionStatus::HARDWARE_ERROR);
}

bool Engine::decode_read_(size_t num_bits, uint8_t *out, size_t bit_offset) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  uint16_t largest_one = 0;
  uint16_t smallest_zero = UINT16_MAX;
  for (size_t i = 0; i < this->rx_count_; i++) {
    const uint16_t low = this->rx_[i].duration0;
    largest_one = std::max<uint16_t>(largest_one, low < this->timing_.read_threshold_us ? low : 0);
    smallest_zero = std::min<uint16_t>(smallest_zero, low < this->timing_.read_threshold_us ? UINT16_MAX : low);
  }
  ESP_LOGV(TAG, "Read section: largest 1 Low %u us, smallest 0 Low %u us", largest_one, smallest_zero);
#endif
  return decode_read_slots(this->rx_, this->rx_count_, num_bits, out, bit_offset, this->timing_);
}

void Engine::finish_(TransactionStatus status) {
  Transaction *done = this->head_;
  this->head_ = done->next_;
  this->step_ = Step::LINE_UNCHECKED;
  done->pending_ = false;
  // Comes last, on_done may submit() again
  if (done->on_done != nullptr)
    done->on_done(done->context, status);
}

bool Engine::start_section_(size_t count, uint32_t idle_us) {
  const bool with_rx = idle_us != NO_RX;
  this->since_ms_ = this->port_->now_ms();
  this->pending_ = with_rx ? PENDING_TX | PENDING_RX : PENDING_TX;
  if (with_rx && !this->port_->arm_rx(this->rx_, this->mem_words_, idle_us)) {
    this->pending_ = 0;
    return false;
  }
  if (this->port_->start_tx(this->tx_, count))
    return true;
  this->pending_ = 0;
  if (with_rx)  // RX stays armed otherwise, and the next arm_rx() would fail
    this->abort_section_();
  return false;
}

bool Engine::abort_section_() {
  ESP_LOGW(TAG, "RMT section did not finish, aborting it");
  this->pending_ = 0;  // Discards whatever the aborted section still reports
  if (!this->port_->abort())
    this->failed_ = true;
  return !this->failed_;
}

void Engine::drain_() {
  while (this->head_ != nullptr) {
    this->poll();
    if (this->head_ != nullptr)
      this->port_->wait_for_event(BLOCKING_WAIT_MS);
  }
}

bool Engine::run_section_(size_t count, uint32_t idle_us) {
  if (this->failed_ || !this->start_section_(count, idle_us))
    return false;
  while (this->pending_ != 0) {
    if (this->port_->now_ms() - this->since_ms_ >= SECTION_TIMEOUT_MS) {
      this->abort_section_();
      return false;
    }
    this->port_->wait_for_event(BLOCKING_WAIT_MS);
  }
  return true;
}

int Engine::reset_blocking() {
  this->drain_();
  this->tx_[0] = reset_symbol();
  if (!this->run_section_(1, RX_IDLE_RESET_US))
    return -1;
  const TransactionStatus status = decode_reset(this->rx_, this->rx_count_);
  if (status == TransactionStatus::CAPTURE_ERROR) {
    ESP_LOGW(TAG, "Reset capture is invalid");
  }
  return status == TransactionStatus::OK ? 1 : (status == TransactionStatus::NO_PRESENCE ? 0 : -1);
}

bool Engine::write_blocking(const uint8_t *data, size_t num_bits) {
  this->drain_();
  const size_t room = max_tx_slots(this->mem_words_);
  for (size_t sent = 0; sent < num_bits;) {
    const size_t count = encode_write_slots(data, sent, std::min(room, num_bits - sent), this->tx_, this->timing_);
    if (!this->run_section_(count, NO_RX))
      return false;
    sent += count;
  }
  return true;
}

bool Engine::read_blocking(uint8_t *data, size_t num_bits) {
  this->drain_();
  const size_t room = max_rx_slots(this->mem_words_);
  for (size_t decoded = 0; decoded < num_bits;) {
    const size_t count = encode_read_slots(std::min(room, num_bits - decoded), this->tx_, this->timing_);
    if (!this->run_section_(count, this->rx_idle_us_) || !this->decode_read_(count, data, decoded)) {
      for (size_t bit = 0; bit < num_bits; bit++)
        data[bit / 8] |= 1u << (bit % 8);
      return false;
    }
    decoded += count;
  }
  return true;
}

void Engine::reset_search() {
  this->search_address_ = 0;
  this->last_discrepancy_ = 0;
  this->last_device_ = false;
}

uint64_t Engine::search_next() {
  this->drain_();
  if (this->last_device_)
    return 0;

  uint64_t address = this->search_address_;
  uint8_t last_zero = 0;
  uint8_t branch = 0;
  for (uint8_t position = 1; position <= 64; position++) {
    // One section: the branch of the previous position, then this position's bit and complement
    size_t count = position > 1 ? encode_write_slots(&branch, 0, 1, this->tx_, this->timing_) : 0;
    count += encode_read_slots(2, this->tx_ + count, this->timing_);
    uint8_t slots = 0;
    if (!this->run_section_(count, this->rx_idle_us_) ||
        !decode_read_slots(this->rx_, this->rx_count_, count, &slots, 0, this->timing_)) {
      ESP_LOGW(TAG, "Search stopped at bit %u", position);
      return 0;
    }
    const bool bit = (slots >> (count - 2)) & 1;
    const bool complement = (slots >> (count - 1)) & 1;
    if (bit && complement)
      return 0;  // No device takes part

    const uint64_t mask = uint64_t{1} << (position - 1);
    if (bit != complement) {
      branch = bit;
    } else {
      // Devices on both branches
      branch = position < this->last_discrepancy_ ? (address & mask) != 0 : position == this->last_discrepancy_;
      if (branch == 0)
        last_zero = position;
    }
    address = branch != 0 ? address | mask : address & ~mask;
  }

  if (!this->run_section_(encode_write_slots(&branch, 0, 1, this->tx_, this->timing_), NO_RX))
    return 0;
  this->last_discrepancy_ = last_zero;
  this->last_device_ = last_zero == 0;
  this->search_address_ = address;
  return address;
}

}  // namespace esphome::gpio::rmt_one_wire

#endif  // USE_ONE_WIRE_RMT
