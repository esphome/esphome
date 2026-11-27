#pragma once
// Wire decoder shared by dev/snf.hpp and ard/swp.hpp. Each sample is a level state: SDA in bit 0, SCL in bit 1.
#include <cstdint>
#include <cstdio>
#include <string>

namespace wiredecode {
// Decodes `count` level states that follow `start` into tokens (`S`, `Sr`, `P`, `90+`, `rA0-`); adds to amb the
// samples where both lines changed at once.
inline std::string decode(const uint8_t *buf, uint32_t count, uint8_t start, uint32_t &amb) {
  std::string out;
  uint8_t prev = start;
  bool open = false, first = false, reading = false;
  int bits = -1;  // -1: no frame; 0..8 bit index in the current 9-bit unit
  uint16_t sh = 0;
  char t[8];
  for (uint32_t i = 0; i < count; i++) {
    uint8_t cur = buf[i];
    bool psda = prev & 1, pscl = prev & 2, sda = cur & 1, scl = cur & 2;
    if ((psda != sda) && (pscl != scl))
      amb++;
    if (pscl && scl && psda && !sda) {
      out += open ? "Sr " : "S ";
      open = true;
      bits = 0;
      sh = 0;
      first = true;
    } else if (pscl && scl && !psda && sda) {
      out += "P ";
      open = false;
      bits = -1;
    } else if (!pscl && scl && bits >= 0) {
      if (bits < 8) {
        sh = (sh << 1) | (sda ? 1 : 0);
        bits++;
      } else {
        uint8_t b = sh & 0xFF;
        if (first)
          reading = b & 1;
        snprintf(t, sizeof t, (!first && reading) ? "r%02X%c " : "%02X%c ", b, sda ? '-' : '+');
        out += t;
        first = false;
        bits = 0;
        sh = 0;
      }
    }
    prev = cur;
  }
  return out;
}
}  // namespace wiredecode
