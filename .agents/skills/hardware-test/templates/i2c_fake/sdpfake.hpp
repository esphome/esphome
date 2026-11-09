#pragma once
// Fake Sensirion SDP3x / SDP8xx sensors on an in-firmware I2C bus. A command the fake does not know and a read it
// does not answer get a NACK and count in bad_cmds and bad_reads; each injected ID read NACK counts in id_fail.
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace sdpfake {
static const char *const TAG = "sdpfake";
using esphome::i2c::ErrorCode;

struct Dev {
  uint8_t addr;
  uint16_t pn_hi, pn_lo;  // product number words
  int16_t dp, temp, scale;
  int fail_read_every;  // every Nth measurement read NACKs
  bool fail_id;         // the ID read NACKs
  bool measuring{false};
  uint16_t last{0}, start{0};
  uint32_t stops{0}, stop_nak{0}, resets{0}, reset_nak{0}, reads{0}, read_fail{0}, bad_cmds{0}, bad_reads{0},
      id_fail{0};
};

struct Bus final : public esphome::i2c::I2CBus {
  Dev devs[6] = {
      {0x21, 0x0301, 0x0101, 1200, 5000, 60, 0, false},   // SDP31-500Pa, 20 Pa
      {0x22, 0x0301, 0x0102, -480, 5000, 240, 0, false},  // SDP32-125Pa, -2 Pa (mass_flow mode)
      {0x25, 0x0302, 0x020A, 3000, 5000, 60, 0, false},   // SDP810-500Pa, 50 Pa
      {0x26, 0x0302, 0x0299, 600, 5000, 60, 0, false},    // unknown product
      {0x23, 0x0301, 0x0101, 1200, 5000, 60, 3, false},   // every 3rd read fails
      {0x24, 0x0301, 0x0101, 1200, 5000, 60, 0, true},    // ID read fails
  };

  static void put(uint8_t *out, size_t &o, uint16_t w) {
    esphome::put_be16(out + o, w);
    out[o + 2] = esphome::crc8(out + o, 2, 0xFF, 0x31, true);
    o += 3;
  }

  ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) override {
    for (auto &d : devs)
      if (d.addr == address)
        return dev_(d, data, size, out, out_size);
    return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
  }

  ErrorCode dev_(Dev &d, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) {
    if (size == 1 || size > 2) {
      d.bad_cmds++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (size == 2) {
      uint16_t c = (data[0] << 8) | data[1];
      if (c == 0x3FF9) {  // stop: refused while idle
        d.stops++;
        if (!d.measuring) {
          d.stop_nak++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
        d.measuring = false;
      } else if (c == 0x0006) {  // soft reset belongs on the general call address
        d.resets++;
        d.reset_nak++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      } else if (c == 0x3603 || c == 0x3615) {
        d.start = c;
        d.measuring = true;
        ESP_LOGI(TAG, "0x%02X start 0x%04X", d.addr, c);
      } else if (c == 0xE102 && d.fail_id) {
        d.id_fail++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      } else if (c != 0x367C && c != 0xE102) {
        d.bad_cmds++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      d.last = c;
    }
    if (out_size > 0) {
      size_t o = 0;
      if (d.last == 0xE102 && out_size == 18) {
        if (d.fail_id) {
          d.id_fail++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
        put(out, o, d.pn_hi);
        put(out, o, d.pn_lo);
        put(out, o, 0x1234);
        put(out, o, 0x5678);
        put(out, o, 0x9ABC);
        put(out, o, 0xDEF0);
      } else if (d.measuring && out_size == 9) {
        d.reads++;
        if (d.fail_read_every && d.reads % d.fail_read_every == 0) {
          d.read_fail++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
        put(out, o, (uint16_t) d.dp);
        put(out, o, (uint16_t) d.temp);
        put(out, o, (uint16_t) d.scale);
      } else {
        d.bad_reads++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
    }
    return esphome::i2c::ERROR_OK;
  }

  void report() {
    for (auto &d : devs)
      ESP_LOGI(TAG,
               "0x%02X start=0x%04X stops=%u(nak %u) resets=%u(nak %u) reads=%u fail=%u id_fail=%u "
               "bad_cmds=%u bad_reads=%u",
               d.addr, d.start, (unsigned) d.stops, (unsigned) d.stop_nak, (unsigned) d.resets, (unsigned) d.reset_nak,
               (unsigned) d.reads, (unsigned) d.read_fail, (unsigned) d.id_fail, (unsigned) d.bad_cmds,
               (unsigned) d.bad_reads);
  }
};
inline Bus bus;
}  // namespace sdpfake
