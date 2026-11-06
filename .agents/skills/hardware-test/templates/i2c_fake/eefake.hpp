#pragma once
// Fake HTE501 (0x40) and TEE501 (0x48) on an in-firmware I2C bus; the temperature word comes from `raw`.
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace eefake {
static const char *const TAG = "eefake";

struct Bus final : public esphome::i2c::I2CBus {
  uint16_t raw{2400};
  uint16_t hum{4512};
  uint8_t last_cmd[2][2]{};  // per address: 0 = 0x40, 1 = 0x48

  static void word(uint8_t *out, uint16_t v) {
    out[0] = v >> 8;
    out[1] = v;
    out[2] = esphome::crc8(out, 2, 0xFF, 0x31, true);
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x40 && address != 0x48)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    uint8_t *cmd = this->last_cmd[address == 0x48];
    if (size >= 2) {
      cmd[0] = data[0];
      cmd[1] = data[1];
    }
    if (out_size == 0)
      return esphome::i2c::ERROR_OK;
    uint8_t buf[9] = {};
    if (cmd[0] == 0x70 && cmd[1] == 0x29) {
      const uint8_t serial[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, address};
      memcpy(buf, serial, 8);
      buf[8] = esphome::crc8(buf, 8, 0xFF, 0x31, true);
    } else if (cmd[0] == 0x2C && cmd[1] == 0x1B) {
      word(buf, this->raw);
      word(buf + 3, this->hum);
    }
    memcpy(out, buf, out_size < sizeof(buf) ? out_size : sizeof(buf));
    return esphome::i2c::ERROR_OK;
  }
};
inline Bus bus;

// Raw temperature words to step through and the value a signed hundredths decode gives
inline const uint16_t RAWS[] = {2400, 13500, 0xFFFF, 0xF060, 0xD8F1, 0xD8F0, 0xC568, 0x8000, 0x0000, 0x7FFF};
inline int idx = 0;
}  // namespace eefake
