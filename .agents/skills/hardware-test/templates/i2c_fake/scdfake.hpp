#pragma once
// Fake SCD4x (0x62 and 0x63) on an in-firmware I2C bus, Sensirion words with CRC; logs every 0xE000 write.
// A 0xE000 write with a bad CRC counts in bad_crc_e000, not count_e000. A write that is not a bare command (2 bytes)
// or a command with one CRC word (5 bytes), a command the scd4x driver does not send or sent with the wrong length,
// or any other command with a bad CRC, gets a NACK and counts in bad_writes.
#include <cmath>
#include <cstring>
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
  uint32_t bad_writes{0};
  uint32_t unknown_reads{0};  // reads after a command the fake does not answer, or longer than its reply, NACKed
  uint32_t e000_writes() const { return this->count_e000[0] + this->count_e000[1]; }
  uint32_t e000_bad_crc() const { return this->bad_crc_e000[0] + this->bad_crc_e000[1]; }

  // write length of each command the scd4x driver sends: 2 for a bare command, 5 for one with an argument; 0 unknown
  static size_t cmd_size(uint16_t c) {
    switch (c) {
      case 0x3682:  // get serial number
      case 0x21B1:  // start periodic measurement
      case 0x21AC:  // start low power periodic measurement
      case 0x219D:  // measure single shot
      case 0x2196:  // measure single shot, RHT only
      case 0xE4B8:  // get data ready status
      case 0xEC05:  // read measurement
      case 0x3F86:  // stop periodic measurement
      case 0x3632:  // factory reset
        return 2;
      case 0x241D:  // temperature offset
      case 0x2427:  // altitude compensation
      case 0x2416:  // automatic self calibration
      case 0x362F:  // forced recalibration
      case 0xE000:  // ambient pressure
        return 5;
      default:
        return 0;
    }
  }

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
    if (size == 1 || size == 3 || size == 4 || size > 5) {
      ESP_LOGW(TAG, "0x%02X malformed write of %u bytes", address, (unsigned) size);
      this->bad_writes++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (size >= 2) {
      uint16_t c = (data[0] << 8) | data[1];
      if (cmd_size(c) != size) {
        ESP_LOGW(TAG, "0x%02X unknown command 0x%04X or wrong length %u", address, c, (unsigned) size);
        this->bad_writes++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      this->cmd[a] = c;
      bool crc_ok = size != 5 || data[4] == esphome::crc8(data + 2, 2, 0xFF, 0x31, true);
      if (!crc_ok && this->cmd[a] != 0xE000) {
        ESP_LOGW(TAG, "0x%02X command 0x%04X with a bad CRC", address, this->cmd[a]);
        this->bad_writes++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      if (this->cmd[a] == 0xE000 && size == 5) {
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
        if (out_size > 0) {
          memset(out, 0, out_size);
          this->unknown_reads++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
        break;
    }
    if (n < out_size) {
      memset(out, 0, out_size);
      this->unknown_reads++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    return esphome::i2c::ERROR_OK;
  }
};
inline Bus bus;
inline const float CASES[] = {1013.6f, 101325.0f, 500.0f,  NAN,      700.0f,   1200.0f, 1013.4f,
                              1200.4f, 699.6f,    1012.5f, 1012.49f, 98000.0f, 0.0f,    1013.0f};
inline int idx = 0;
inline float case_p = NAN;
inline int case_expect = -1;  // E000 value the case should send to 0x62, -1 for none
// counters when the case started
inline uint32_t case_e000 = 0, case_bad_crc = 0, case_bad_writes = 0, case_unknown = 0;

// true for a build that skips values outside 700-1200 hPa (0 allowed) and rounds (#21935); dev truncates
static constexpr bool RANGE_CHECKED = false;
// 0x63's fixed ambient_pressure_compensation, which setup sends once
static constexpr float FIXED_HPA = 1013.6f;
inline uint16_t sent = 0;  // mirror of the 0x62 driver's last pressure

// E000 value the driver should send for p, or -1 for none; updates the mirror
inline int expect_e000(float p) {
  if (std::isnan(p) || (RANGE_CHECKED && p != 0.0f && (p < 700.0f || p > 1200.0f)))
    return -1;
  uint16_t v = RANGE_CHECKED ? (uint16_t) lroundf(p) : (uint16_t) (int32_t) p;
  if (v == sent)
    return -1;
  sent = v;
  return v;
}

// counts since the case started; PASS only when nothing is bad, the E000 writes match and CO2 was published
inline void log_case(const char *kind, float p, int co2) {  // co2: 1 published, 0 missing, -1 not read
  uint32_t writes = bus.e000_writes() - case_e000, bad_crc = bus.e000_bad_crc() - case_bad_crc;
  uint32_t bad_writes = bus.bad_writes - case_bad_writes, unknown = bus.unknown_reads - case_unknown;
  bool e000_ok = case_expect < 0 ? writes == 0 : writes == 1 && bus.last_e000[0] == case_expect;
  bool pass = e000_ok && co2 != 0 && bad_crc == 0 && bad_writes == 0 && unknown == 0;
  ESP_LOGI(TAG, "CASE %s %.2f -> %u E000 writes (expected %d), %u bad CRC, %u bad writes, %u unknown reads, co2 %s %s",
           kind, p, (unsigned) writes, case_expect, (unsigned) bad_crc, (unsigned) bad_writes, (unsigned) unknown,
           co2 > 0 ? "ok" : (co2 == 0 ? "MISSING" : "not read"), pass ? "PASS" : "FAIL");
}

inline void start_case(float p) {
  case_p = p;
  case_expect = expect_e000(p);
  case_e000 = bus.e000_writes();
  case_bad_crc = bus.e000_bad_crc();
  case_bad_writes = bus.bad_writes;
  case_unknown = bus.unknown_reads;
}
}  // namespace scdfake
