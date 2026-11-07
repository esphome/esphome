#pragma once
// Fake SCD4x (0x62 and 0x63) on an in-firmware I2C bus, Sensirion words with CRC; logs every 0xE000 write.
// A 0xE000 write with a bad CRC counts in bad_crc_e000, not count_e000.
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace scdfake {
static const char *const TAG = "scdfake";

struct Bus final : public esphome::i2c::I2CBus {
  uint16_t cmd[2]{};
  int last_e000[2]{-1, -1};
  uint32_t count_e000[2]{};
  uint32_t bad_crc_e000[2]{};
  uint32_t e000_writes() const { return this->count_e000[0] + this->count_e000[1]; }
  uint32_t e000_bad_crc() const { return this->bad_crc_e000[0] + this->bad_crc_e000[1]; }

  static void put(uint8_t *out, size_t &n, size_t max, uint16_t v) {
    uint8_t w[3];
    esphome::put_be16(w, v);
    w[2] = esphome::crc8(w, 2, 0xFF, 0x31, true);
    for (uint8_t b : w)
      if (n < max)
        out[n++] = b;
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                                      size_t out_size) override {
    if (address != 0x62 && address != 0x63)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    int a = address - 0x62;
    if (size >= 2) {
      this->cmd[a] = (data[0] << 8) | data[1];
      if (this->cmd[a] == 0xE000 && size >= 5) {
        bool crc_ok = data[4] == esphome::crc8(data + 2, 2, 0xFF, 0x31, true);
        if (crc_ok) {
          this->last_e000[a] = (data[2] << 8) | data[3];
          this->count_e000[a]++;
        } else {
          this->bad_crc_e000[a]++;
        }
        ESP_LOGI(TAG, "0x%02X E000 <- %u hPa (crc %s)", address, (data[2] << 8) | data[3], crc_ok ? "ok" : "BAD");
      }
    }
    size_t n = 0;
    switch (this->cmd[a]) {
      case 0x3682:  // serial number
        put(out, n, out_size, 0x1234);
        put(out, n, out_size, 0x5678);
        put(out, n, out_size, 0x9ABC);
        break;
      case 0xE4B8:  // data ready
        put(out, n, out_size, 0x8006);
        break;
      case 0xEC05:  // measurement: 600 ppm, ~22 C, ~45 %
        put(out, n, out_size, 600);
        put(out, n, out_size, 0x6666);
        put(out, n, out_size, 0x7333);
        break;
      default:
        break;
    }
    while (n < out_size)
      put(out, n, out_size, 0);
    return esphome::i2c::ERROR_OK;
  }
};
inline Bus bus;
inline const float CASES[] = {1013.6f, 101325.0f, 500.0f,  NAN,      700.0f,   1200.0f, 1013.4f,
                              1200.4f, 699.6f,    1012.5f, 1012.49f, 98000.0f, 0.0f,    1013.0f};
inline int idx = 0;
}  // namespace scdfake
