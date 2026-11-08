#pragma once
// Times Bluedroid's sscanf parsing against esphome#22081's replacements on the same inputs.
#include <cctype>
#include <cstdio>
#include <cstring>
#include <esp_cpu.h>
#include "esphome/components/esp32_ble/bluedroid_hex.h"
#include "esphome/core/log.h"

// Bluedroid string_to_bdaddr (bdaddr.c)
static bool __attribute__((noinline)) ref_bdaddr(const char *string, uint8_t *addr) {
  unsigned int p[6];
  bool ok = sscanf(string, "%02x:%02x:%02x:%02x:%02x:%02x", &p[0], &p[1], &p[2], &p[3], &p[4], &p[5]) == 6;
  if (ok)
    for (int i = 0; i < 6; i++)
      addr[i] = (uint8_t) p[i];
  return ok;
}
// Bluedroid btc_config_get_bin decode loop (btc_config.c), after its length and hex checks
static bool __attribute__((noinline)) ref_bin(const char *str, size_t len, uint8_t *value, size_t *length) {
  if (len % 2 || len / 2 > *length)
    return false;
  for (size_t i = 0; i < len; i++)
    if (!isxdigit((unsigned char) str[i]))
      return false;
  for (size_t i = 0; i < len / 2; i++) {
    unsigned int val;
    sscanf(str + i * 2, "%02x", &val);
    value[i] = (uint8_t) val;
  }
  *length = len / 2;
  return true;
}
static bool __attribute__((noinline)) new_bdaddr(const char *s, uint8_t *a) {
  return esphome::esp32_ble::parse_bdaddr(s, a);
}
static bool __attribute__((noinline)) new_bin(const char *s, size_t n, uint8_t *v, size_t *l) {
  return esphome::esp32_ble::parse_config_bin(s, n, v, l);
}

inline void hex_bench() {
  static constexpr const char *ADDRS[] = {"12:34:56:78:9A:BC", "aa:bb:cc:dd:ee:ff", "1:2:3:4:5:6", "00:11:22:33:44:5G"};
  // LTK (16 bytes), IRK (16), a 28 byte bonding blob
  static constexpr const char *BINS[] = {"00112233445566778899aabbccddeeff", "F0E1D2C3B4A5968778695A4B3C2D1E0F",
                                         "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c"};
  const int n = 2000;
  int mism = 0;
  for (const char *s : ADDRS) {
    uint8_t a[6] = {}, b[6] = {};
    bool ra = ref_bdaddr(s, a), rb = new_bdaddr(s, b);
    if (ra != rb || (ra && memcmp(a, b, 6) != 0)) {
      mism++;
      ESP_LOGW("bench", "bdaddr mismatch on %s: ref %d new %d", s, ra, rb);
    }
    uint32_t t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < n; i++)
      ref_bdaddr(s, a);
    uint32_t t1 = esp_cpu_get_cycle_count();
    for (int i = 0; i < n; i++)
      new_bdaddr(s, b);
    uint32_t t2 = esp_cpu_get_cycle_count();
    ESP_LOGI("bench", "bdaddr %-17s ok=%d sscanf %5u cycles, new %4u cycles", s, (int) rb, (unsigned) ((t1 - t0) / n),
             (unsigned) ((t2 - t1) / n));
  }
  for (const char *s : BINS) {
    uint8_t a[32] = {}, b[32] = {};
    size_t la = 32, lb = 32, len = strlen(s);
    bool ra = ref_bin(s, len, a, &la), rb = new_bin(s, len, b, &lb);
    if (ra != rb || la != lb || memcmp(a, b, la) != 0) {
      mism++;
      ESP_LOGW("bench", "bin mismatch on %s", s);
    }
    uint32_t t0 = esp_cpu_get_cycle_count();
    for (int i = 0; i < n; i++) {
      la = 32;
      ref_bin(s, len, a, &la);
    }
    uint32_t t1 = esp_cpu_get_cycle_count();
    for (int i = 0; i < n; i++) {
      lb = 32;
      new_bin(s, len, b, &lb);
    }
    uint32_t t2 = esp_cpu_get_cycle_count();
    ESP_LOGI("bench", "bin %2u bytes sscanf %5u cycles, new %4u cycles", (unsigned) lb, (unsigned) ((t1 - t0) / n),
             (unsigned) ((t2 - t1) / n));
  }
  ESP_LOGI("bench", "DONE mismatches=%d", mism);
}
