// Test harness for the virtual_uart component (#20428): a fake MH-Z19 and SDS011 behind virtual UARTs.
#pragma once
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <vector>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace vu {
static const char *const TAG = "vutest";

struct Stats {
  // mhz19
  uint32_t mhz_tx_cmds = 0, mhz_tx_getppm = 0, mhz_tx_other = 0, mhz_tx_bad = 0, mhz_tx_bytes = 0, mhz_tx_calls = 0;
  uint32_t mhz_state_ok = 0, mhz_state_bad = 0, mhz_temp_ok = 0, mhz_temp_bad = 0;
  uint16_t mhz_expect_ppm = 0;
  // sds011
  uint32_t sds_tx_frames = 0, sds_tx_ok = 0, sds_tx_bad = 0, sds_tx_calls = 0;
  uint32_t sds_static_ok = 0, sds_lambda_ok = 0, sds_ovf_ok = 0, sds_state_bad = 0, sds_ovf_forbidden = 0;
  float sds_lambda_expect = NAN;
  uint32_t sds_lambda_n = 0;
  // overflow on vu_sds
  uint32_t ovf_runs = 0, ovf_skip = 0, ovf_ok = 0, ovf_bad = 0;
  // raw buffer checks on vu_raw
  uint32_t raw_runs = 0, raw_ok = 0, raw_bad = 0;
  // self write
  uint32_t self_tx_calls = 0;
};
inline Stats st;

inline void hex(const char *tag, const uint8_t *d, size_t n) {
  char buf[esphome::format_hex_pretty_size(19)];
  ESP_LOGI(TAG, "%s (%u) %s", tag, (unsigned) n, esphome::format_hex_pretty_to(buf, d, n, ' '));
}

// ---- MH-Z19 model: collects the 9 command bytes and answers GET_PPM with a fresh ppm ----
inline uint8_t mhz_tx_buf[9];
inline uint8_t mhz_tx_len = 0;
static const uint8_t GET_PPM[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
static const uint8_t ABC_OFF[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};

template<typename U> void on_mhz_tx(U *u, std::span<const uint8_t> data) {
  st.mhz_tx_calls++;
  st.mhz_tx_bytes += data.size();
  for (uint8_t b : data) {
    if (mhz_tx_len < 9)
      mhz_tx_buf[mhz_tx_len++] = b;
  }
  if (mhz_tx_len < 9)
    return;
  mhz_tx_len = 0;
  st.mhz_tx_cmds++;
  hex("MHZ TX", mhz_tx_buf, 9);
  if (memcmp(mhz_tx_buf, GET_PPM, 9) == 0) {
    st.mhz_tx_getppm++;
    uint16_t ppm = 400 + (st.mhz_tx_getppm * 37) % 4000;
    uint8_t r[9] = {0xFF, 0x86, (uint8_t) (ppm >> 8), (uint8_t) ppm, 40 + 23, 0x00, 0, 0, 0};
    uint8_t s = 0;
    for (int i = 1; i < 8; i++)
      s += r[i];
    r[8] = -s;
    st.mhz_expect_ppm = ppm;
    u->inject(r, 9);
  } else if (memcmp(mhz_tx_buf, ABC_OFF, 9) == 0) {
    st.mhz_tx_other++;
  } else {
    st.mhz_tx_bad++;
  }
}
inline void on_mhz_state(float x) {
  if (x == st.mhz_expect_ppm)
    st.mhz_state_ok++;
  else {
    st.mhz_state_bad++;
    ESP_LOGW(TAG, "MHZ state %.0f, expected %u", x, st.mhz_expect_ppm);
  }
}
inline void on_mhz_temp(float x) {
  if (x == 23)
    st.mhz_temp_ok++;
  else
    st.mhz_temp_bad++;
}

// ---- SDS011: checks the setup commands it writes ----
inline uint8_t sds_tx_buf[19];
inline uint8_t sds_tx_len = 0;
static const uint8_t SDS_MODE[19] = {0xAA, 0xB4, 0x02, 0x01, 0x00, 0,    0,    0,    0,   0,
                                     0,    0,    0,    0,    0,    0xFF, 0xFF, 0x01, 0xAB};
static const uint8_t SDS_PERIOD[19] = {0xAA, 0xB4, 0x08, 0x01, 0x00, 0,    0,    0,    0,   0,
                                       0,    0,    0,    0,    0,    0xFF, 0xFF, 0x07, 0xAB};

inline void on_sds_tx(std::span<const uint8_t> data) {
  st.sds_tx_calls++;
  for (uint8_t b : data) {
    if (sds_tx_len < 19)
      sds_tx_buf[sds_tx_len++] = b;
  }
  if (sds_tx_len < 19)
    return;
  sds_tx_len = 0;
  st.sds_tx_frames++;
  hex("SDS TX", sds_tx_buf, 19);
  if (memcmp(sds_tx_buf, SDS_MODE, 19) == 0 || memcmp(sds_tx_buf, SDS_PERIOD, 19) == 0)
    st.sds_tx_ok++;
  else
    st.sds_tx_bad++;
}

inline std::array<uint8_t, 10> sds_frame(uint16_t pm25x10, uint16_t pm10x10) {
  std::array<uint8_t, 10> f = {
      0xAA, 0xC0, (uint8_t) pm25x10, (uint8_t) (pm25x10 >> 8), (uint8_t) pm10x10, (uint8_t) (pm10x10 >> 8), 0x12, 0x34,
      0,    0xAB};
  uint8_t s = 0;
  for (int i = 2; i < 8; i++)
    s += f[i];
  f[8] = s;
  return f;
}
// lambda frames: pm2.5 = 100.0 + n/10 so every frame has its own value
inline std::vector<uint8_t> sds_next() {
  st.sds_lambda_n++;
  uint16_t v = 1000 + (st.sds_lambda_n % 2000);
  st.sds_lambda_expect = v / 10.0f;
  auto f = sds_frame(v, 555);
  return {f.begin(), f.end()};  // inject_rx takes a vector
}
inline void on_sds_state(float x) {
  if (std::fabs(x - 12.3f) < 0.01f)
    st.sds_static_ok++;
  else if (std::fabs(x - 88.8f) < 0.01f || std::fabs(x - 99.9f) < 0.01f)
    st.sds_ovf_ok++;
  else if (std::fabs(x - 77.7f) < 0.01f)
    st.sds_ovf_forbidden++;
  else if (std::fabs(x - st.sds_lambda_expect) < 0.01f)
    st.sds_lambda_ok++;
  else {
    st.sds_state_bad++;
    ESP_LOGW(TAG, "SDS state %.1f, expected %.1f", x, st.sds_lambda_expect);
  }
}

// ---- overflow on vu_sds (rx 20): 21 bytes dropped, 20 fit, then 1 more dropped ----
template<typename U> void overflow(U *u) {
  st.ovf_runs++;
  if (u->available() != 0) {
    st.ovf_skip++;
    return;
  }
  uint8_t a[21] = {};
  auto fa = sds_frame(777, 555);
  memcpy(a, fa.data(), fa.size());
  u->inject(a, sizeof(a));  // must drop, with a warning
  size_t after_a = u->available();
  uint8_t b[20];
  auto fb = sds_frame(888, 555), fc = sds_frame(999, 555);
  memcpy(b, fb.data(), 10);
  memcpy(b + 10, fc.data(), 10);
  u->inject(b, sizeof(b));  // exactly fits
  size_t after_b = u->available();
  uint8_t one = 0xAA;
  u->inject(&one, 1);  // full, must drop
  size_t after_c = u->available();
  bool ok = after_a == 0 && after_b == 20 && after_c == 20;
  if (ok)
    st.ovf_ok++;
  else
    st.ovf_bad++;
  ESP_LOGI(TAG, "OVF avail after 21B=%u after 20B=%u after 1B=%u %s", (unsigned) after_a, (unsigned) after_b,
           (unsigned) after_c, ok ? "ok" : "BAD");
}

// ---- vu_raw (rx 128, no reader): static 100 + 20 then 9 dropped, 8 fit, read back 0..127 ----
template<typename U> void raw_check(U *u, bool log) {
  st.raw_runs++;
  size_t n = u->available();
  uint8_t buf[128];
  bool ok = n == 128 && u->read_array(buf, 128);
  for (int i = 0; ok && i < 128; i++)
    ok = buf[i] == i;
  if (ok)
    st.raw_ok++;
  else {
    st.raw_bad++;
    ESP_LOGW(TAG, "RAW avail %u, content mismatch", (unsigned) n);
    uint8_t junk;
    while (u->available() && u->read_array(&junk, 1)) {
    }
  }
  if (log)
    ESP_LOGI(TAG, "RAW avail=%u %s", (unsigned) n, ok ? "ok" : "BAD");
}

inline void summary() {
  ESP_LOGI(TAG,
           "SUM mhz tx_calls=%u tx_bytes=%u cmds=%u getppm=%u abc=%u bad=%u state_ok=%u state_bad=%u "
           "temp_ok=%u temp_bad=%u",
           st.mhz_tx_calls, st.mhz_tx_bytes, st.mhz_tx_cmds, st.mhz_tx_getppm, st.mhz_tx_other, st.mhz_tx_bad,
           st.mhz_state_ok, st.mhz_state_bad, st.mhz_temp_ok, st.mhz_temp_bad);
  ESP_LOGI(TAG,
           "SUM sds tx_calls=%u frames=%u ok=%u bad=%u static_ok=%u lambda_n=%u lambda_ok=%u ovf_states=%u "
           "forbidden=%u bad_state=%u",
           st.sds_tx_calls, st.sds_tx_frames, st.sds_tx_ok, st.sds_tx_bad, st.sds_static_ok, st.sds_lambda_n,
           st.sds_lambda_ok, st.sds_ovf_ok, st.sds_ovf_forbidden, st.sds_state_bad);
  ESP_LOGI(TAG, "SUM ovf runs=%u skip=%u ok=%u bad=%u raw runs=%u ok=%u bad=%u self_tx_calls=%u", st.ovf_runs,
           st.ovf_skip, st.ovf_ok, st.ovf_bad, st.raw_runs, st.raw_ok, st.raw_bad, st.self_tx_calls);
}
}  // namespace vu
