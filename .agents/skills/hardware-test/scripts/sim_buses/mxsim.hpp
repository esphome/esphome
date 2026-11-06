#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/core/log.h"

// Sits in front of a MAX7219 chain's real SPI delegate and keeps what each chip of the chain would latch.
class MxSniff : public esphome::spi::SPIDelegate {
 public:
  MxSniff(esphome::spi::SPIDelegate *real, uint8_t chips)
      : SPIDelegate(1000000, esphome::spi::BIT_ORDER_MSB_FIRST, esphome::spi::MODE0, nullptr),
        real_(real),
        chips_(chips) {}
  uint8_t digits[8][8]{};  // [chip nearest DIN first][register 1..8 - 1]
  uint8_t ctrl[8][16]{};
  uint32_t transactions{0}, bad{0};
  void begin_transaction() override {
    this->n_ = 0;
    this->real_->begin_transaction();
  }
  void end_transaction() override {
    this->real_->end_transaction();
    this->transactions++;
    if (this->n_ != this->chips_ * 2) {
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
  void report(const char *name) {
    char line[8 * 8 * 3 + 1];
    char *o = line;
    for (uint8_t c = 0; c < this->chips_; c++)
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
  esphome::spi::SPIDelegate *real_;
  uint8_t chips_;
  uint8_t bytes_[16]{};
  size_t n_{0};
};

struct DelegatePeek : esphome::spi::SPIClient {
  static esphome::spi::SPIDelegate *&of(esphome::spi::SPIClient *c) {
    return static_cast<DelegatePeek *>(c)->delegate_;
  }
};
