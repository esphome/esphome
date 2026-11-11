#pragma once
// crc32 vectors for esphome#22398, expected values from Python zlib.crc32
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>
namespace crctest {
inline int pass_n = 0, fail_n = 0;
inline void check(const char *name, uint32_t got, uint32_t want) {
  if (got == want)
    pass_n++;
  else
    fail_n++;
  ESP_LOGI("crctest", "%s %s got=%08X want=%08X", got == want ? "PASS" : "FAIL", name, (unsigned) got, (unsigned) want);
}
// unqualified: a crc32() of "123456789" computed in the caller's lambda, or -1 to skip that check
inline void run(int64_t unqualified = -1) {
  pass_n = fail_n = 0;
  const uint8_t *s = reinterpret_cast<const uint8_t *>("123456789");
  check("check-123456789", esphome::crc32(0, s, 9), 0xCBF43926u);
  check("empty-seed0", esphome::crc32(0, s, 0), 0u);
  check("empty-seed12345678", esphome::crc32(0x12345678u, s, 0), 0x12345678u);
  check("chained-4+5", esphome::crc32(esphome::crc32(0, s, 4), s + 4, 5), 0xCBF43926u);
  uint8_t all[256];
  for (int i = 0; i < 256; i++)
    all[i] = i;
  check("bytes-0-255", esphome::crc32(0, all, 256), 0x29058C73u);
  uint8_t ff[32];
  memset(ff, 0xFF, sizeof(ff));
  check("32x0xFF", esphome::crc32(0, ff, 32), 0xFF6CAB0Bu);
  uint8_t *pat = static_cast<uint8_t *>(malloc(4100 + 8192));
  if (pat == nullptr) {
    ESP_LOGE("crctest", "malloc failed");
    fail_n++;
    ESP_LOGI("crctest", "CRCRESULT pass=%d fail=%d", pass_n, fail_n);
    return;
  }
  for (int i = 0; i < 4100; i++)
    pat[i] = (i * 7 + 3) & 0xFF;
  static const uint32_t EXP_OFFSET[4] = {0xDF4C2BA9u, 0x5D4B54F6u, 0x3B885695u, 0xAE989C6Eu};
  for (int o = 0; o < 4; o++) {
    char n[24];
    snprintf(n, sizeof(n), "unaligned-off%d", o);
    check(n, esphome::crc32(0, pat + o, 4093), EXP_OFFSET[o]);
  }
  uint32_t c = 0;
  for (int i = 0; i < 300; i++)
    c = esphome::crc32(c, pat + i, 1);
  check("chained-1-byte-x300", c, 0xDE0E57CEu);
  c = 0;
  for (int i = 0; i < 4093; i += 97)
    c = esphome::crc32(c, pat + 1 + i, std::min(97, 4093 - i));
  check("chained-97-byte-unaligned", c, 0x5D4B54F6u);
  uint8_t *big = pat + 4100;
  for (int i = 0; i < 8192; i++)
    big[i] = (i * 13 + 5) & 0xFF;
  uint32_t best = 0xFFFFFFFF, r = 0;
  for (int k = 0; k < 5; k++) {
    const uint32_t t0 = (esphome::micros) ();
    r = esphome::crc32(0, big, 8192);
    const uint32_t dt = (esphome::micros) () - t0;
    if (dt < best)
      best = dt;
  }
  check("8KiB", r, 0xF6950930u);
  free(pat);
  ESP_LOGI("crctest", "CRCSPEED 8192 bytes in %u us = %.1f us/KiB", (unsigned) best, best / 8.0f);
  if (unqualified >= 0)
    check("unqualified", (uint32_t) unqualified, 0xCBF43926u);
  ESP_LOGI("crctest", "CRCRESULT pass=%d fail=%d", pass_n, fail_n);
}
}  // namespace crctest
