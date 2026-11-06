#pragma once
// 4 byte distance frames (a01nyub, a02yyuw, jsn_sr04t): FF H L sum
#include "sim.hpp"
namespace sim {
inline std::vector<uint8_t> d4(uint16_t mm, bool bad_ck = false) {
  std::vector<uint8_t> f = {0xFF, (uint8_t) (mm >> 8), (uint8_t) mm, 0};
  f[3] = sum(f, 0, 3) + (bad_ck ? 1 : 0);
  return f;
}
}  // namespace sim
