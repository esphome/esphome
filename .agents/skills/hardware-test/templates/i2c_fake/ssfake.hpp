#pragma once
// Fake Adafruit seesaw chips on an in-firmware I2C bus: status, GPIO bulk, ADC, touch, encoder and NeoPixel modules.
// A transfer is [module, register, data...]; a read in its own transaction answers the last register written.
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstring>
#include <string>

namespace ssfake {
static const char *const TAG = "ssfake";
using esphome::i2c::ErrorCode;

struct Chip {
  uint8_t addr;
  uint8_t hw_id;
  bool nack{false};               // every transfer NACKs
  int nack_reads{0};              // NACK this many reads
  int nack_writes{0};             // NACK this many writes
  int nack_np{0};                 // NACK this many NeoPixel module writes
  uint32_t temp_raw{0x00198000};  // 25.5 C in 16.16; 0xFFFFFFFF means no sensor
  uint32_t version{(1234UL << 16) | (12 << 11) | (3 << 7) | 26};
  uint32_t options{0x0003C2E3};
  // GPIO
  uint32_t dir{0}, pull{0}, out{0}, ext_mask{0}, ext_level{0};
  // ADC by channel, touch by channel, encoder raw positions
  uint16_t adc[16]{};
  uint16_t touch[16]{};
  int32_t enc[16]{};
  uint32_t enc_inten{0};
  // NeoPixel
  uint8_t np_pin{0xFF}, np_speed{0xFF};
  uint16_t np_len{0};
  uint8_t np_buf[512]{};
  uint32_t np_shows{0}, np_chunks{0}, np_big{0};
  // bookkeeping
  uint8_t mod{0xFF}, reg{0xFF};
  uint32_t t_write{0};
  uint32_t gap[0x20][2]{};  // last write to read gap in us, by module: [mod][0] gap, [1] reads
  uint32_t bulk_reads{0}, writes{0}, reads{0}, bad{0}, nacked{0};
  std::string log;
};

struct Bus final : public esphome::i2c::I2CBus {
  Chip chips[3] = {{0x36, 0x87}, {0x49, 0x55}, {0x4A, 0x87, true}};

  Chip *find(uint8_t a) {
    for (auto &c : chips)
      if (c.addr == a)
        return &c;
    return nullptr;
  }

  uint32_t gpio_in(Chip &c) {
    uint32_t v = 0;
    for (int p = 0; p < 32; p++) {
      uint32_t m = 1UL << p;
      bool lvl;
      if (c.dir & m)
        lvl = c.out & m;
      else if (c.ext_mask & m)
        lvl = c.ext_level & m;
      else if (c.pull & m)
        lvl = c.out & m;  // pull direction follows the output latch
      else
        lvl = false;
      if (lvl)
        v |= m;
    }
    return v;
  }

  ErrorCode write_readv(uint8_t address, const uint8_t *d, size_t n, uint8_t *out, size_t out_size) override {
    Chip *cp = find(address);
    if (cp == nullptr)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    Chip &c = *cp;
    if (c.nack) {
      c.nacked++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (n > 0) {
      if (c.nack_writes > 0) {
        c.nack_writes--;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      if (n < 2) {
        c.bad++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      if (d[0] == 0x0E && c.nack_np > 0) {
        c.nack_np--;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      if (n > 32)
        c.np_big++;
      c.writes++;
      c.mod = d[0];
      c.reg = d[1];
      c.t_write = esphome::micros();
      const uint8_t *v = d + 2;
      size_t vn = n - 2;
      // a write the fake does not model is NACKed, so the driver sees it fail when it happens
      auto nak = [&c]() {
        c.bad++;
        c.log += " ";
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      };
      char tmp[24];
      snprintf(tmp, sizeof(tmp), "%02X.%02X", d[0], d[1]);
      c.log += tmp;
      if (d[0] == 0x01 && vn == 4) {  // GPIO bulk writes
        uint32_t m = esphome::encode_uint32(v[0], v[1], v[2], v[3]);
        switch (d[1]) {
          case 0x02:
            c.dir |= m;
            break;
          case 0x03:
            c.dir &= ~m;
            break;
          case 0x05:
            c.out |= m;
            break;
          case 0x06:
            c.out &= ~m;
            break;
          case 0x0B:
            c.pull |= m;
            break;
          case 0x0C:
            c.pull &= ~m;
            break;
          default:
            return nak();
        }
        snprintf(tmp, sizeof(tmp), "=%X", (unsigned) m);
        c.log += tmp;
      } else if (d[0] == 0x11 && d[1] >= 0x10 && d[1] < 0x20 && vn == 1) {
        if (v[0] & 1)
          c.enc_inten |= 1UL << (d[1] - 0x10);
      } else if (d[0] == 0x0E) {
        switch (d[1]) {
          case 0x01:
            if (vn == 1)
              c.np_pin = v[0];
            else
              return nak();
            break;
          case 0x02:
            if (vn == 1)
              c.np_speed = v[0];
            else
              return nak();
            break;
          case 0x03:
            if (vn == 2)
              c.np_len = (v[0] << 8) | v[1];
            else
              return nak();
            break;
          case 0x04: {
            if (vn < 2)
              return nak();
            uint16_t off = (v[0] << 8) | v[1];
            if (off + vn - 2 > sizeof(c.np_buf))
              return nak();
            memcpy(c.np_buf + off, v + 2, vn - 2);
            c.np_chunks++;
            break;
          }
          case 0x05:
            if (vn == 0)
              c.np_shows++;
            else
              return nak();
            break;
          default:
            return nak();
        }
      } else if (vn != 0) {
        return nak();
      }
      c.log += " ";
    }
    if (out_size > 0) {
      if (c.nack_reads > 0) {
        c.nack_reads--;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      c.reads++;
      uint32_t g = esphome::micros() - c.t_write;
      if (c.mod < 0x20) {
        c.gap[c.mod][0] = g;
        c.gap[c.mod][1]++;
      }
      memset(out, 0xEE, out_size);
      if (c.mod == 0x00) {
        if (c.reg == 0x01 && out_size == 1)
          out[0] = c.hw_id;
        else if (c.reg == 0x02 && out_size == 4)
          esphome::put_be32(out, c.version);
        else if (c.reg == 0x03 && out_size == 4)
          esphome::put_be32(out, c.options);
        else if (c.reg == 0x04 && out_size == 4)
          esphome::put_be32(out, c.temp_raw);
        else {
          c.bad++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
      } else if (c.mod == 0x01 && c.reg == 0x04 && out_size == 4) {
        esphome::put_be32(out, gpio_in(c));
        c.bulk_reads++;
      } else if (c.mod == 0x09 && c.reg >= 0x07 && c.reg < 0x17 && out_size == 2) {
        uint16_t a = c.adc[c.reg - 0x07];
        out[0] = a >> 8;
        out[1] = a;
      } else if (c.mod == 0x0F && c.reg >= 0x10 && c.reg < 0x20 && out_size == 2) {
        uint16_t t = c.touch[c.reg - 0x10];
        out[0] = t >> 8;
        out[1] = t;
      } else if (c.mod == 0x11 && c.reg >= 0x30 && c.reg < 0x40 && out_size == 4) {
        esphome::put_be32(out, (uint32_t) c.enc[c.reg - 0x30]);
      } else {
        c.bad++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
    }
    return esphome::i2c::ERROR_OK;
  }

  std::string pixels(Chip &c, int n) {
    std::string s;
    char tmp[8];
    for (int i = 0; i < n * 3; i++) {
      snprintf(tmp, sizeof(tmp), "%02X", c.np_buf[i]);
      s += tmp;
      if (i % 3 == 2 && i != n * 3 - 1)
        s += " ";
    }
    return s;
  }
};
inline Bus bus;

inline uint32_t pass_n = 0, fail_n = 0;
inline void check(const char *name, const std::string &got, const std::string &want) {
  bool ok = got == want;
  (ok ? pass_n : fail_n)++;
  if (ok)
    ESP_LOGI(TAG, "PASS %s [%s]", name, got.c_str());
  else
    ESP_LOGE(TAG, "FAIL %s got [%s] want [%s]", name, got.c_str(), want.c_str());
}
inline void check(const char *name, int64_t got, int64_t want) {
  check(name, std::to_string(got), std::to_string(want));
}
inline void check_true(const char *name, bool ok, const std::string &info) {
  (ok ? pass_n : fail_n)++;
  if (ok)
    ESP_LOGI(TAG, "PASS %s [%s]", name, info.c_str());
  else
    ESP_LOGE(TAG, "FAIL %s [%s]", name, info.c_str());
}
inline std::string f2s(float v) {
  char b[24];
  snprintf(b, sizeof(b), "%.2f", v);
  return b;
}
}  // namespace ssfake
