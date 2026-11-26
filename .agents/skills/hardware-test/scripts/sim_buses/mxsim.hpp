#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/core/log.h"

// Sits in front of a MAX7219 chain's real SPI delegate and keeps what each chip of the chain would latch.
class MxSniff : public esphome::spi::SPIDelegate {
 public:
  static constexpr uint8_t MAX_CHIPS = 8;  // a longer chain counts every transaction as bad
  MxSniff(esphome::spi::SPIDelegate *real, uint8_t chips)
      : SPIDelegate(1000000, esphome::spi::BIT_ORDER_MSB_FIRST, esphome::spi::MODE0, nullptr),
        real_(real),
        chips_(chips) {}
  uint8_t digits[MAX_CHIPS][8]{};  // [chip nearest DIN first][register 1..8 - 1]
  uint8_t ctrl[MAX_CHIPS][16]{};
  uint32_t transactions{0}, bad{0};
  void begin_transaction() override {
    this->n_ = 0;
    this->odd_ = false;
    this->real_->begin_transaction();
  }
  void end_transaction() override {
    this->real_->end_transaction();
    this->transactions++;
    if (this->chips_ > MAX_CHIPS || this->odd_ || this->n_ != this->chips_ * 2) {
      this->bad++;
      return;
    }
    // the last pair shifted in stays in the chip nearest DIN
    for (uint8_t p = 0; p < this->chips_; p++) {
      uint8_t chip = this->chips_ - 1 - p, reg = this->bytes_[p * 2], val = this->bytes_[p * 2 + 1];
      if (reg >= 1 && reg <= 8)
        this->digits[chip][reg - 1] = val;
      else
        this->ctrl[chip][reg & 0x0F] = val;
    }
  }
  uint8_t transfer(uint8_t data) override {
    this->rec_(data);
    return this->real_->transfer(data);
  }
  void write_array(const uint8_t *ptr, size_t length) override {
    for (size_t i = 0; i < length; i++)
      this->rec_(ptr[i]);
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
    for (size_t i = 0; i < length; i++)
      this->rec_(data[i]);
    this->real_->write_cmd_addr_data(cmd_bits, cmd, addr_bits, address, data, length, bus_width);
  }
  bool is_ready() override { return this->real_->is_ready(); }
  void report(const char *name) {
    char line[MAX_CHIPS * 8 * 3 + 1];
    char *o = line;
    *o = '\0';
    for (uint8_t c = 0; c < this->chips_ && c < MAX_CHIPS; c++)
      for (uint8_t r = 0; r < 8; r++)
        o += sprintf(o, "%02X%s", this->digits[c][r], r == 7 ? "|" : " ");
    ESP_LOGI("mxsim", "MX %s tx=%u bad=%u shutdown=%u scan=%u intensity=%u digits=%s", name,
             (unsigned) this->transactions, (unsigned) this->bad, this->ctrl[0][0x0C], this->ctrl[0][0x0B],
             this->ctrl[0][0x0A], line);
  }

 protected:
  void rec_(uint8_t b) {
    if (this->n_ < sizeof(this->bytes_))
      this->bytes_[this->n_] = b;
    this->n_++;
  }
  // whole bytes MSB first; a partial byte makes the transaction bad
  void rec_bits_(uint32_t v, size_t bits) {
    this->odd_ |= bits % 8 != 0;
    for (size_t s = bits - bits % 8; s >= 8; s -= 8)
      this->rec_(v >> (s - 8));
  }
  esphome::spi::SPIDelegate *real_;
  uint8_t chips_;
  uint8_t bytes_[MAX_CHIPS * 2]{};
  size_t n_{0};
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
