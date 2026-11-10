#pragma once
// Fake HTE501 (0x40) and TEE501 (0x48) on an in-firmware I2C bus; the temperature word comes from `raw`.
// A command other than 0x7029 (identification) and 0x2C1B (measure), and a read longer than the command's reply, get a
// NACK and count in bad; measures counts the measure replies each address returned.
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace eefake {
static const char *const TAG = "eefake";

struct Bus final : public esphome::i2c::I2CBus {
  uint16_t raw{2400};
  uint16_t hum{4512};
  uint8_t last_cmd[2][2]{};  // per address: 0 = 0x40, 1 = 0x48
  uint32_t bad{0};
  uint32_t measures[2]{};  // measure replies read, per address

  static void word(uint8_t *out, uint16_t v) {
    esphome::put_be16(out, v);
    out[2] = esphome::crc8(out, 2, 0xFF, 0x31, true);
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x40 && address != 0x48)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    uint8_t *cmd = this->last_cmd[address == 0x48];
    if (size == 1 || size > 2 ||
        (size == 2 && !((data[0] == 0x70 && data[1] == 0x29) || (data[0] == 0x2C && data[1] == 0x1B)))) {
      this->bad++;
      if (out_size)
        memset(out, 0, out_size);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (size == 2) {
      cmd[0] = data[0];
      cmd[1] = data[1];
    }
    if (out_size == 0)
      return esphome::i2c::ERROR_OK;
    uint8_t buf[9] = {};
    size_t len = 0;
    if (cmd[0] == 0x70 && cmd[1] == 0x29) {
      const uint8_t serial[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, address};
      memcpy(buf, serial, 8);
      buf[8] = esphome::crc8(buf, 8, 0xFF, 0x31, true);
      len = 9;
    } else if (cmd[0] == 0x2C && cmd[1] == 0x1B) {
      word(buf, this->raw);
      word(buf + 3, this->hum);
      len = 6;
    }
    if (out_size > len) {  // no command yet, or longer than the reply
      memset(out, 0, out_size);
      this->bad++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    memcpy(out, buf, out_size);
    if (cmd[0] == 0x2C)
      this->measures[address == 0x48]++;
    return esphome::i2c::ERROR_OK;
  }
};
inline Bus bus;

// Raw temperature words to step through and the value a signed hundredths decode gives
inline const uint16_t RAWS[] = {2400, 13500, 0xFFFF, 0xF060, 0xD8F1, 0xD8F0, 0xC568, 0x8000, 0x0000, 0x7FFF};
inline int idx = 0;
inline uint32_t case_measures[2]{};
}  // namespace eefake
