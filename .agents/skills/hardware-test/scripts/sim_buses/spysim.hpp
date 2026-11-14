#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Records each SPI transaction's length and FNV-1a hash in front of the real delegate.
class SpySniff : public esphome::spi::SPIDelegate {
 public:
  explicit SpySniff(esphome::spi::SPIDelegate *real)
      : SPIDelegate(8000000, esphome::spi::BIT_ORDER_MSB_FIRST, esphome::spi::MODE0, nullptr), real_(real) {}
  uint32_t tx{0}, odd{0}, last_len{0}, last_hash{0};
  void begin_transaction() override {
    this->len_ = 0;
    this->hash_ = esphome::FNV1_OFFSET_BASIS;
    this->odd_ = false;
    this->real_->begin_transaction();
  }
  void end_transaction() override {
    this->real_->end_transaction();
    this->tx++;
    if (this->odd_)
      this->odd++;
    this->last_len = this->len_;
    this->last_hash = this->hash_;
    if (this->len_ > 1000)
      ESP_LOGI("spysim", "SPY frame len=%u hash=%08X odd=%u", (unsigned) this->len_, (unsigned) this->hash_,
               (unsigned) this->odd);
  }
  uint8_t transfer(uint8_t data) override {
    this->rec_(&data, 1);
    return this->real_->transfer(data);
  }
  void write_array(const uint8_t *ptr, size_t length) override {
    this->rec_(ptr, length);
    this->real_->write_array(ptr, length);
  }
  void write(uint16_t data, size_t num_bits) override {
    this->rec_bits_(data, num_bits);
    this->real_->write(data, num_bits);
  }
  void write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address, const uint8_t *data,
                           size_t length, uint8_t bus_width) override {
    this->odd_ |= bus_width != 1;
    this->rec_bits_(cmd, cmd_bits);
    this->rec_bits_(address, addr_bits);
    this->rec_(data, length);
    this->real_->write_cmd_addr_data(cmd_bits, cmd, addr_bits, address, data, length, bus_width);
  }
  bool is_ready() override { return this->real_->is_ready(); }

 protected:
  void rec_(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++)
      this->hash_ = (this->hash_ ^ p[i]) * esphome::FNV1_PRIME;
    this->len_ += n;
  }
  // a partial byte is hashed as a whole one and makes the transaction odd
  void rec_bits_(uint32_t v, size_t bits) {
    this->odd_ |= bits % 8 != 0;
    for (size_t s = (bits + 7) / 8 * 8; s >= 8; s -= 8) {
      uint8_t b = v >> (s - 8);
      this->rec_(&b, 1);
    }
  }
  esphome::spi::SPIDelegate *real_;
  uint32_t len_{0}, hash_{0};
  bool odd_{false};
};
// Shared by mxsim.hpp and spysim.hpp, so a config can include both.
#ifndef SIM_BUSES_DELEGATE_PEEK
#define SIM_BUSES_DELEGATE_PEEK
struct DelegatePeek : esphome::spi::SPIClient {
  static esphome::spi::SPIDelegate *&of(esphome::spi::SPIClient *c) {
    return static_cast<DelegatePeek *>(c)->delegate_;
  }
};
#endif
