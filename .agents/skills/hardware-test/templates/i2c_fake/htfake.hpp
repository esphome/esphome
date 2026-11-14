#pragma once
// Fake HT16K33 chips (Adafruit 0.56" 7-segment backpack) on an in-firmware I2C bus, plus a test runner for
// esphome::ht16k33_char. Records every command, keeps the 16 byte display RAM per chip and renders it back.
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"
#include <cstring>
#include <string>

namespace htfake {
static const char *const TAG = "htfake";
using esphome::i2c::ErrorCode;

struct Chip {
  uint8_t addr;
  bool nack{false};
  uint8_t ram[16]{};
  bool osc{false};
  uint8_t disp{0};    // last display setup command
  uint8_t dim{0xFF};  // last dimming command
  uint32_t frames{0}, cmds{0}, bad{0}, nacked{0};
  std::string log;  // command trace, cleared by the test
};

// 7-segment byte to character; digits win over look-alike letters
inline const char *seg_char(uint8_t s) {
  switch (s) {
    case 0x00:
      return " ";
    case 0x3F:
      return "0";
    case 0x06:
      return "1";
    case 0x5B:
      return "2";
    case 0x4F:
      return "3";
    case 0x66:
      return "4";
    case 0x6D:
      return "5";
    case 0x7D:
      return "6";
    case 0x07:
      return "7";
    case 0x7F:
      return "8";
    case 0x6F:
      return "9";
    case 0x40:
      return "-";
    case 0x08:
      return "_";
    case 0x77:
      return "A";
    case 0x39:
      return "C";
    case 0x79:
      return "E";
    case 0x71:
      return "F";
    case 0x3D:
      return "G";
    case 0x76:
      return "H";
    case 0x30:
      return "I";
    case 0x0E:
      return "J";
    case 0x38:
      return "L";
    case 0x37:
      return "N";
    case 0x73:
      return "P";
    case 0x3E:
      return "U";
    case 0x6E:
      return "Y";
    case 0x7C:
      return "b";
    case 0x58:
      return "c";
    case 0x5E:
      return "d";
    case 0x74:
      return "h";
    case 0x5C:
      return "o";
    case 0x50:
      return "r";
    case 0x78:
      return "t";
    case 0x1C:
      return "u";
    case 0x63:
      return "*";  // degree sign from add_characters
    default:
      return "?";
  }
}

struct Bus final : public esphome::i2c::I2CBus {
  Chip chips[8] = {{0x70}, {0x71}, {0x72}, {0x73}, {0x74, true}, {0x75}, {0x76, true}, {0x77}};
  uint32_t unknown{0};

  Chip *find(uint8_t a) {
    for (auto &c : chips)
      if (c.addr == a)
        return &c;
    return nullptr;
  }

  ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) override {
    Chip *c = find(address);
    if (c == nullptr) {
      unknown++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (c->nack) {
      c->nacked++;
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (out_size) {  // the driver never reads
      c->bad++;
      memset(out, 0, out_size);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (size == 0)
      return esphome::i2c::ERROR_OK;
    char tmp[8];
    uint8_t cmd = data[0];
    if ((cmd & 0xF0) == 0x00) {
      // display data: address pointer then bytes, auto increment
      uint8_t p = cmd & 0x0F;
      for (size_t i = 1; i < size; i++)
        c->ram[(p + i - 1) & 0x0F] = data[i];
      if (size != 17 || p != 0)
        c->bad++;
      c->frames++;
      c->log += "D ";
    } else {
      if (size != 1)
        c->bad++;
      if ((cmd & 0xF0) == 0x20)
        c->osc = cmd & 1;
      else if ((cmd & 0xF0) == 0x80)
        c->disp = cmd;
      else if ((cmd & 0xF0) == 0xE0)
        c->dim = cmd;
      else
        c->bad++;
      c->cmds++;
      snprintf(tmp, sizeof(tmp), "%02X ", cmd);
      c->log += tmp;
    }
    return esphome::i2c::ERROR_OK;
  }

  // "12:34" style rendering; unused RAM bytes that are not zero show as "!"
  std::string render(uint8_t a) {
    Chip *c = find(a);
    std::string s;
    static const uint8_t off[4] = {0, 2, 6, 8};
    for (int d = 0; d < 4; d++) {
      s += seg_char(c->ram[off[d]] & 0x7F);
      if (c->ram[off[d]] & 0x80)
        s += ".";
      if (d == 1 && (c->ram[4] & 0x02))
        s += ":";
    }
    for (int i = 0; i < 16; i++) {
      bool used = i == 0 || i == 2 || i == 6 || i == 8;
      if (i == 4) {
        if (c->ram[4] & ~0x02)
          s += "!";
      } else if (!used && c->ram[i])
        s += "!";
    }
    return s;
  }
  void clear_logs() {
    for (auto &c : chips)
      c.log.clear();
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
inline void check(const char *name, int got, int want) { check(name, std::to_string(got), std::to_string(want)); }
inline std::string r3(uint8_t a = 0x70) { return bus.render(a) + "|" + bus.render(a + 1) + "|" + bus.render(a + 2); }
}  // namespace htfake
