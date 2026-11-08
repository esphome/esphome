#pragma once
// Per trigger counts of the received payloads: AAnn (service data) and BBnn (manufacturer data) from the advertiser,
// anything else counted as "other".
#include <cstdint>
#include <cstdio>
#include <vector>
#include "esphome/core/log.h"

namespace rx {
static constexpr int TRIGGERS = 12;
static uint32_t cnt[TRIGGERS][16];
static uint32_t other[TRIGGERS];
static uint32_t all, ours;

inline void hit(int t, const std::vector<uint8_t> &x) {
  if (x.size() == 2 && (x[0] == 0xAA || x[0] == 0xBB) && x[1] >= 1 && x[1] <= 7) {
    cnt[t][(x[0] == 0xBB ? 8 : 0) + x[1]]++;
  } else {
    other[t]++;
  }
}

inline void report(const char *label) {
  for (int t = 1; t < TRIGGERS; t++) {
    char buf[160];
    int n = snprintf(buf, sizeof(buf), "T%d:", t);
    for (int k = 0; k < 16; k++) {
      if (cnt[t][k])
        n += snprintf(buf + n, sizeof(buf) - n, " %s%02u=%u", k >= 8 ? "BB" : "AA", (unsigned) (k & 7),
                      (unsigned) cnt[t][k]);
    }
    if (other[t])
      n += snprintf(buf + n, sizeof(buf) - n, " other=%u", (unsigned) other[t]);
    ESP_LOGI("rx", "%s %s", label, buf);
  }
}
}  // namespace rx
