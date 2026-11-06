#pragma once
// Fake SX1509 hubs (0x3E, 0x3F) on an in-firmware I2C bus: register files with reset values, every write recorded.
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

namespace sxfake {
static const char *const TAG = "sxfake";

struct Write {
  uint8_t addr, reg, val;
};

struct Bus final : public esphome::i2c::I2CBus {
  uint8_t regs[2][128];
  uint8_t reset_step[2]{};
  Write log[600];
  uint16_t n{0};
  uint32_t dropped{0};

  Bus() { this->reset_(0), this->reset_(1); }
  void reset_(int a) {
    memset(this->regs[a], 0, 128);
    this->regs[a][0x0E] = this->regs[a][0x0F] = 0xFF;  // RegDirB/A
    this->regs[a][0x10] = this->regs[a][0x11] = 0xFF;  // RegDataB/A
    this->regs[a][0x12] = this->regs[a][0x13] = 0xFF;  // RegInterruptMaskB/A
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x3E && address != 0x3F)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    int a = address - 0x3E;
    if (size == 0 && out_size == 0)
      return esphome::i2c::ERROR_OK;
    uint8_t reg = size ? data[0] : 0;
    for (size_t i = 1; i < size; i++, reg++) {
      uint8_t r = reg & 0x7F;
      if (this->n < 600)
        this->log[this->n++] = {address, (uint8_t) (data[0] + i - 1), data[i]};
      else
        this->dropped++;
      if (data[0] == 0x7D) {  // RegReset: 0x12 then 0x34 resets the chip
        if (data[i] == 0x12)
          this->reset_step[a] = 1;
        else if (data[i] == 0x34 && this->reset_step[a] == 1)
          this->reset_(a), this->reset_step[a] = 0;
        continue;
      }
      this->regs[a][r] = data[i];
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->regs[a][(data[0] + i) & 0x7F];
    return esphome::i2c::ERROR_OK;
  }

  void dump_log(uint16_t from) {
    char line[160];
    size_t p = 0;
    for (uint16_t i = from; i < this->n; i++) {
      p += snprintf(line + p, sizeof(line) - p, "%02X:%02X=%02X ", this->log[i].addr, this->log[i].reg,
                    this->log[i].val);
      if (p > sizeof(line) - 12 || i + 1 == this->n) {
        ESP_LOGI(TAG, "W %s", line);
        p = 0;
      }
    }
    if (this->dropped)
      ESP_LOGW(TAG, "dropped %u writes", this->dropped);
  }

  void dump_regs() {
    for (int a = 0; a < 2; a++) {
      for (int base = 0; base < 0x80; base += 32) {
        char line[100];
        size_t p = 0;
        for (int i = 0; i < 32; i++)
          p += snprintf(line + p, sizeof(line) - p, "%02X", this->regs[a][base + i]);
        ESP_LOGI(TAG, "REGS %02X %02X: %s", 0x3E + a, base, line);
      }
    }
  }
};
inline Bus bus;
}  // namespace sxfake
