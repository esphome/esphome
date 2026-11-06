#pragma once
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

// Fake HT16K33 at 0x70: keeps the 16 display RAM bytes written after a 0x00 address byte, acks everything.
class HtSimBus : public esphome::i2c::I2CBus {
 public:
  explicit HtSimBus(esphome::i2c::I2CBus *real) : real_(real) {}
  uint8_t ram[16]{};
  uint32_t ram_writes{0}, cmds{0};
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) override {
    if (address != 0x70)
      return this->real_->write_readv(address, w, wn, r, rn);
    if (wn > 1 && w[0] <= 0x0F) {
      memcpy(this->ram + w[0], w + 1, std::min<size_t>(wn - 1, 16 - w[0]));
      this->ram_writes++;
    } else if (wn) {
      this->cmds++;
    }
    if (rn)
      memset(r, 0, rn);
    return esphome::i2c::ERROR_OK;
  }
  void report() {
    char hex[16 * 3 + 1];
    for (int i = 0; i < 16; i++)
      sprintf(hex + i * 3, "%02X ", this->ram[i]);
    ESP_LOGI("htsim", "HT ram_writes=%u cmds=%u ram=%s", (unsigned) this->ram_writes, (unsigned) this->cmds, hex);
  }

 protected:
  esphome::i2c::I2CBus *real_;
};
inline HtSimBus *g_ht = nullptr;
