#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/core/log.h"

// Records each SPI transaction's length and FNV-1a hash in front of the real delegate.
class SpySniff : public esphome::spi::SPIDelegate {
 public:
  explicit SpySniff(esphome::spi::SPIDelegate *real)
      : SPIDelegate(8000000, esphome::spi::BIT_ORDER_MSB_FIRST, esphome::spi::MODE0, nullptr), real_(real) {}
  uint32_t tx{0}, last_len{0}, last_hash{0};
  void begin_transaction() override {
    this->len_ = 0;
    this->hash_ = 2166136261u;
    this->real_->begin_transaction();
  }
  void end_transaction() override {
    this->real_->end_transaction();
    this->tx++;
    this->last_len = this->len_;
    this->last_hash = this->hash_;
    if (this->len_ > 1000)
      ESP_LOGI("spysim", "SPY frame len=%u hash=%08X", (unsigned) this->len_, (unsigned) this->hash_);
  }
  uint8_t transfer(uint8_t data) override {
    this->rec_(&data, 1);
    return this->real_->transfer(data);
  }
  void write_array(const uint8_t *ptr, size_t length) override {
    this->rec_(ptr, length);
    this->real_->write_array(ptr, length);
  }

 protected:
  void rec_(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++)
      this->hash_ = (this->hash_ ^ p[i]) * 16777619u;
    this->len_ += n;
  }
  esphome::spi::SPIDelegate *real_;
  uint32_t len_{0}, hash_{0};
};
struct DelegatePeek : esphome::spi::SPIClient {
  static esphome::spi::SPIDelegate *&of(esphome::spi::SPIClient *c) {
    return static_cast<DelegatePeek *>(c)->delegate_;
  }
};
