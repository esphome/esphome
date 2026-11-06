#pragma once
// Fake GT911 on an in-firmware I2C bus: switches, max values, touch state and key byte.
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

namespace gtfake {
struct Bus final : public esphome::i2c::I2CBus {
  uint16_t reg{0};
  uint8_t keys{0}, touches{0};
  bool ready{false};
  uint32_t clears{0}, polls{0};
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x5D)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size >= 2) {
      this->reg = (data[0] << 8) | data[1];
      if (this->reg == 0x814E && size == 3) {
        this->ready = false;
        this->clears++;
      }
    }
    if (out_size) {
      memset(out, 0, out_size);
      switch (this->reg) {
        case 0x804D:
          out[0] = 0x00;
          break;  // switches: rising edge interrupt
        case 0x8048:
          out[0] = 0xE0;
          out[1] = 0x01;
          out[2] = 0x10;
          out[3] = 0x01;
          break;  // 480 x 272
        case 0x814E:
          this->polls++;
          out[0] = this->ready ? (0x80 | (this->keys ? 0x10 : 0) | this->touches) : 0;
          break;
        case 0x814F:
          for (uint8_t t = 0; t < this->touches && (t + 1) * 8 <= out_size; t++) {
            out[t * 8 + 0] = t;  // track id
            out[t * 8 + 1] = 100;
            out[t * 8 + 3] = 50;  // x 100, y 50
          }
          if (this->touches * 8 < out_size)
            out[this->touches * 8] = this->keys;
          break;
      }
    }
    return esphome::i2c::ERROR_OK;
  }
  void set(uint8_t k, uint8_t t) {
    this->keys = k;
    this->touches = t;
    this->ready = true;
    ESP_LOGI("gtfake", "set keys=0x%X touches=%u", k, t);
  }
};
inline Bus bus;
}  // namespace gtfake
