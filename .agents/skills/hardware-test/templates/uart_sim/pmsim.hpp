#pragma once
// Frame builders for the gcja5, pm1006 and sds011 simulation (esphome#21939 stack).
#include "esphome/core/helpers.h"
#include "sim.hpp"

namespace sim {
// gcja5: 02, PM1/PM2.5/PM10 u32 LE, six u16 LE counts, status, XOR of 1..29, 03
inline std::vector<uint8_t> gcja5(uint32_t pm1, uint32_t pm25, uint32_t pm10, bool bad_ck = false,
                                  bool bad_end = false) {
  std::vector<uint8_t> f(32, 0);
  f[0] = 0x02;
  for (int i = 0; i < 4; i++) {
    f[1 + i] = pm1 >> (8 * i);
    f[5 + i] = pm25 >> (8 * i);
    f[9 + i] = pm10 >> (8 * i);
  }
  f[13] = 100;  // pmc 0.3
  f[15] = 50;   // pmc 0.5
  f[17] = 20;   // pmc 1.0
  f[21] = 5;    // pmc 2.5
  f[23] = 2;    // pmc 5.0
  f[25] = 1;    // pmc 10
  f[29] = 0x00;
  f[30] = esphome::xor_bytes(f.data() + 1, 29) ^ (bad_ck ? 0x01 : 0);
  f[31] = bad_end ? 0x04 : 0x03;
  return f;
}
// pm1006: 16 11 0B DF1..DF16 (PM2.5 at DF3..DF4 big endian) Ck, all bytes sum to zero
inline std::vector<uint8_t> pm1006(uint16_t pm25, bool bad_ck = false) {
  std::vector<uint8_t> f(20, 0);
  f[0] = 0x16;
  f[1] = 0x11;
  f[2] = 0x0B;
  f[5] = pm25 >> 8;
  f[6] = pm25;
  f[19] = (uint8_t) (-sum(f, 0, 19)) + (bad_ck ? 1 : 0);
  return f;
}
// sds011: AA C0 pm25*10 LE, pm10*10 LE, id 2 bytes, sum of 2..7, AB
inline std::vector<uint8_t> sds011(uint16_t pm25x10, uint16_t pm10x10, bool bad_ck = false, bool bad_tail = false) {
  std::vector<uint8_t> f = {
      0xAA, 0xC0, (uint8_t) pm25x10, (uint8_t) (pm25x10 >> 8), (uint8_t) pm10x10, (uint8_t) (pm10x10 >> 8), 0x12, 0x34,
      0,    0xAB};
  f[8] = sum(f, 2, 8) + (bad_ck ? 1 : 0);
  if (bad_tail)
    f[9] = 0xAC;
  return f;
}
}  // namespace sim
