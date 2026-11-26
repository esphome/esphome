#pragma once
// Sensor filter A/B harness: a fixed input sequence goes to every test sensor; each output is logged as its float bits.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include "esphome/core/log.h"

namespace sf {
static constexpr float INF = std::numeric_limits<float>::infinity();
static const float INPUTS[] = {0.0f,    1.0f,   -1.0f,    2.5f,   10.0f, 10.0001f, -10.0f,  100.0f,  1e6f,     -1e6f,
                               3.4e38f, NAN,    INF,      -INF,   0.5f,  0.49f,    5.0f,    5.5f,    4.9f,     6.1f,
                               20.0f,   21.0f,  25.0f,    19.0f,  23.5f, 1000.0f,  4096.0f, 2048.0f, 10000.0f, 0.001f,
                               1e-30f,  -0.0f,  12.3456f, 7.0f,   7.1f,  7.05f,    30.0f,   31.5f,   18.0f,    NAN,
                               15.0f,   110.0f, 110.5f,   0.999f, -5.0f, -5.01f,   50.0f,   50.01f,  2.0f,     2.6f};
static constexpr int COUNT = sizeof(INPUTS) / sizeof(INPUTS[0]);
static constexpr int SLOTS = 40;
static uint32_t bits[SLOTS];
static bool got[SLOTS];
static std::string txt[4];
static bool tgot[4];
static int8_t bin[4];

inline void rec(int k, float v) {
  memcpy(&bits[k], &v, 4);
  got[k] = true;
}
inline void rec_text(int k, const std::string &v) {
  txt[k] = v;
  tgot[k] = true;
}
inline void rec_bin(int k, bool v) { bin[k] = v; }
inline void clear() {
  memset(got, 0, sizeof(got));
  memset(tgot, 0, sizeof(tgot));
  memset(bin, -1, sizeof(bin));
}
inline void line(int idx, int last) {
  char buf[480];
  int n = snprintf(buf, sizeof(buf), "i=%02d", idx);
  for (int k = 1; k <= last && n < (int) sizeof(buf) - 12; k++) {
    if (got[k])
      n += snprintf(buf + n, sizeof(buf) - n, " %08x", (unsigned) bits[k]);
    else
      n += snprintf(buf + n, sizeof(buf) - n, " -");
  }
  ESP_LOGI("sf", "%s", buf);
  n = snprintf(buf, sizeof(buf), "j=%02d b1=%d b2=%d", idx, bin[1], bin[2]);
  for (int k = 1; k <= 2; k++)
    n += snprintf(buf + n, sizeof(buf) - n, " t%d=%s%s%s", k, tgot[k] ? "'" : "-", tgot[k] ? txt[k].c_str() : "",
                  tgot[k] ? "'" : "");
  ESP_LOGI("sf", "%s", buf);
}
}  // namespace sf
