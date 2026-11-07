#pragma once
// Fake SX1509 hubs (0x3E, 0x3F) on an in-firmware I2C bus: register files with reset values, every write recorded.
// A read without a register, and a transfer that runs past 0x7F, gets a NACK and counts in bad.
#include <cstring>
#include <initializer_list>
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
  uint32_t dropped{0}, bad{0};

  Bus() { this->reset_(0), this->reset_(1); }
  void reset_(int a) {
    memset(this->regs[a], 0, 128);
    this->regs[a][0x0E] = this->regs[a][0x0F] = 0xFF;  // RegDirB/A
    this->regs[a][0x10] = this->regs[a][0x11] = 0xFF;  // RegDataB/A
    this->regs[a][0x12] = this->regs[a][0x13] = 0xFF;  // RegInterruptMaskB/A
    this->regs[a][0x27] = this->regs[a][0x28] = 0xFF;  // RegKeyData1/2
    for (uint8_t r : {0x2A, 0x2D, 0x30, 0x33, 0x36, 0x3B, 0x40, 0x45, 0x4A, 0x4D, 0x50, 0x53, 0x56, 0x5B, 0x60, 0x65})
      this->regs[a][r] = 0xFF;  // RegIOn0..15
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x3E && address != 0x3F)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    int a = address - 0x3E;
    if (size == 0 && out_size == 0)
      return esphome::i2c::ERROR_OK;
    // the fake keeps no register pointer between transfers, and the chip has no register past 0x7F
    if (size == 0 || data[0] > 0x7F || data[0] + (size - 1) > 0x80 || data[0] + out_size > 0x80) {
      this->bad++;
      if (out_size)
        memset(out, 0, out_size);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    const uint8_t start = data[0];
    uint8_t r = start;
    for (size_t i = 1; i < size; i++, r++) {
      if (this->n < 600)
        this->log[this->n++] = {address, r, data[i]};
      else
        this->dropped++;
      if (r == 0x7D) {  // RegReset: 0x12 then 0x34 resets the chip
        if (data[i] == 0x12)
          this->reset_step[a] = 1;
        else if (data[i] == 0x34 && this->reset_step[a] == 1)
          this->reset_(a), this->reset_step[a] = 0;
        continue;
      }
      this->regs[a][r] = data[i];
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->regs[a][start + i];
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
      ESP_LOGW(TAG, "dropped %u writes", (unsigned) this->dropped);
    if (this->bad)
      ESP_LOGW(TAG, "%u transfers without a register or past 0x7F, NACKed", (unsigned) this->bad);
    // the fake's own verdict; whether the writes are right is decided by diffing them between builds
    ESP_LOGI(TAG, "END dropped %u bad %u %s", (unsigned) this->dropped, (unsigned) this->bad,
             this->dropped || this->bad ? "FAIL" : "PASS");
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
