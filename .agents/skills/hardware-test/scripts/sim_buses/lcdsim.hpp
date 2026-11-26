#pragma once
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"

// Fake PCF8574 backpacks (P0 RS, P2 EN, P4..P7 D4..D7) driving HD44780 controllers, on top of the real bus.
// Each simulated display keeps the data written after the last 0x80 (row 0, then row 2) and 0xC0 (row 1, then row 3)
// address commands. Data past 128 bytes in a segment counts in overflow. lcd_pcf8574 never reads, so a read gets a NACK
// and counts in reads.
struct LcdSim {
  uint8_t addr{0};
  uint8_t prev{0};
  bool four_bit{false}, have_high{false}, cgram{false};
  uint8_t high{0};
  uint8_t seg{0};
  char segs[2][129]{};
  uint8_t len[2]{};
  uint32_t frames{0}, cmds{0}, data{0}, cgram_bytes{0}, overflow{0}, reads{0};
  void byte(uint8_t rs, uint8_t v) {
    if (!rs) {
      this->cmds++;
      if (v & 0x80) {
        this->cgram = false;
        this->seg = (v & 0x40) ? 1 : 0;
        if (this->seg == 0)
          this->frames++;
        this->len[this->seg] = 0;
        this->segs[this->seg][0] = 0;
      } else if (v & 0x40) {
        this->cgram = true;
      } else if (v == 0x01 || v == 0x02) {
        this->cgram = false;
      } else if ((v & 0xE0) == 0x20 && !this->four_bit && !(v & 0x10)) {
        this->four_bit = true;
        this->have_high = false;
      }
      return;
    }
    if (this->cgram) {
      this->cgram_bytes++;
      return;
    }
    this->data++;
    uint8_t &l = this->len[this->seg];
    if (l < 128) {
      this->segs[this->seg][l++] = (v >= 32 && v < 127) ? (char) v : '?';
      this->segs[this->seg][l] = 0;
    } else {
      this->overflow++;
    }
  }
  void wire(uint8_t b) {
    if ((this->prev & 0x04) && !(b & 0x04)) {
      uint8_t nib = this->prev >> 4, rs = this->prev & 0x01;
      if (!this->four_bit) {
        this->byte(rs, nib << 4);
      } else if (!this->have_high) {
        this->high = nib;
        this->have_high = true;
      } else {
        this->have_high = false;
        this->byte(rs, (this->high << 4) | nib);
      }
    }
    this->prev = b;
  }
};

class LcdSimBus : public esphome::i2c::I2CBus {
 public:
  explicit LcdSimBus(esphome::i2c::I2CBus *real) : real_(real) {}
  static constexpr uint8_t MAX_SIMS = 3;
  LcdSim sims[MAX_SIMS];
  uint8_t n{0};
  void add(uint8_t addr) {
    if (this->n >= MAX_SIMS) {
      ESP_LOGE("lcdsim", "no room for 0x%02X, raise MAX_SIMS", addr);
      return;
    }
    this->sims[this->n++].addr = addr;
  }
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) override {
    for (uint8_t i = 0; i < this->n; i++) {
      if (this->sims[i].addr == address) {
        for (size_t k = 0; k < wn; k++)
          this->sims[i].wire(w[k]);
        if (rn) {
          memset(r, 0xFF, rn);
          this->sims[i].reads++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
        return esphome::i2c::ERROR_OK;
      }
    }
    return this->real_->write_readv(address, w, wn, r, rn);
  }
  void report(uint8_t cols) {
    for (uint8_t i = 0; i < this->n; i++) {
      auto &s = this->sims[i];
      ESP_LOGI("lcdsim", "LCD 0x%02X frames=%u cmds=%u data=%u cgram=%u overflow=%u reads=%u seg0=[%s] seg1=[%s]",
               s.addr, (unsigned) s.frames, (unsigned) s.cmds, (unsigned) s.data, (unsigned) s.cgram_bytes,
               (unsigned) s.overflow, (unsigned) s.reads, s.segs[0], s.segs[1]);
    }
  }

 protected:
  esphome::i2c::I2CBus *real_;
};
inline LcdSimBus *g_lcd = nullptr;
