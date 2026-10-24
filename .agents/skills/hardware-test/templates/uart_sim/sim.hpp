#pragma once
// Simulated UART sensors for the FrameReader hardware test: frame builders, UART loopback and a SEN0465 responder.
#include <driver/uart.h>
#include <cstring>
#include <vector>
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace sim {
using esphome::uart::IDFUARTComponent;
static const char *const TAG = "sim";

inline void loopback(IDFUARTComponent *u) {
  esp_err_t err = uart_set_loop_back((uart_port_t) u->get_hw_serial_number(), true);
  ESP_LOGI(TAG, "UART%u loopback %s", u->get_hw_serial_number(),
           err == ESP_OK ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("FAILED"));
}

inline void send(IDFUARTComponent *u, const char *name, std::vector<uint8_t> bytes) {
  char hex[200] = {0};
  size_t n = 0;
  for (uint8_t b : bytes) {
    if (n + 4 > sizeof(hex))
      break;
    n += snprintf(hex + n, sizeof(hex) - n, "%02X ", b);
  }
  ESP_LOGI(TAG, ">>> %s: %s", name, hex);
  u->write_array(bytes.data(), bytes.size());
}

inline uint8_t sum(const std::vector<uint8_t> &v, size_t from, size_t to) {
  uint8_t s = 0;
  for (size_t i = from; i < to; i++)
    s += v[i];
  return s;
}

// wts01: 55 01 01 04 01 11 T Td Ck
inline std::vector<uint8_t> wts01(uint8_t t, uint8_t td, bool bad_ck = false) {
  std::vector<uint8_t> f = {0x55, 0x01, 0x01, 0x04, 0x01, 0x11, t, td, 0};
  f[8] = sum(f, 0, 8) + (bad_ck ? 1 : 0);
  return f;
}
// ds1603l: FF H L Ck
inline std::vector<uint8_t> ds1603l(uint16_t mm, bool bad_ck = false) {
  std::vector<uint8_t> f = {0xFF, (uint8_t) (mm >> 8), (uint8_t) mm, 0};
  f[3] = sum(f, 0, 3) + (bad_ck ? 1 : 0);
  return f;
}
// sm300d2: 3C 02 co2 hcho tvoc pm25 pm10 (16 bit each) temp temp.dec hum hum.dec Ck
inline std::vector<uint8_t> sm300d2(uint16_t co2, uint8_t temp, uint8_t temp_dec) {
  std::vector<uint8_t> f = {
      0x3C, 0x02, (uint8_t) (co2 >> 8), (uint8_t) co2, 0, 10, 0, 20, 0, 5, 0, 8, temp, temp_dec, 45, 6, 0};
  f[16] = sum(f, 0, 16);
  return f;
}
// ufm01 active frame, temperature t.tt as BCD (t_int, t_frac)
inline std::vector<uint8_t> ufm01(uint8_t t_int_bcd, uint8_t t_frac_bcd, bool bad_ck = false) {
  std::vector<uint8_t> f(32, 0);
  f[0] = 0x3C;
  f[1] = 0x32;
  f[2] = 0x01;
  f[4] = 0x14;
  f[5] = 0x07;
  f[6] = 0x23;
  f[7] = 0x01;
  f[9] = 0x56;  // accumulated flow 0.056
  f[15] = 0x0B;
  f[16] = 0x50;  // flow
  f[21] = 0x0C;
  f[24] = 0x0D;
  f[25] = t_frac_bcd;
  f[26] = t_int_bcd;
  f[31] = 0x16;
  f[30] = sum(f, 0, 30) + (bad_ck ? 1 : 0);
  return f;
}
inline std::vector<uint8_t> cat(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}
inline std::vector<uint8_t> part(const std::vector<uint8_t> &v, size_t from, size_t to) {
  return std::vector<uint8_t>(v.begin() + from, v.begin() + to);
}

// SEN0465 probe on the other end of a cross-wired UART pair. Modes: 0 normal, 1 tail before the answer,
// 2 tail with an FF before the answer, 3 silent, 4 answer split in two 300 ms apart, 5 wrong gas type.
struct Probe {
  IDFUARTComponent *u{nullptr};
  int mode{0};
  uint8_t req[9];
  uint8_t n{0};
  std::vector<uint8_t> rest;
  uint32_t rest_at{0};
  uint16_t conc{25};

  static uint8_t ck(const std::vector<uint8_t> &f) { return (uint8_t) (~sum(f, 1, 7) + 1); }

  void poll() {
    if (!this->rest.empty() && millis() - this->rest_at >= 300) {
      send(this->u, "probe answer rest", this->rest);
      this->rest.clear();
    }
    uint8_t c;
    while (this->u->available() && this->u->read_byte(&c)) {
      if (this->n == 0 && c != 0xFF)
        continue;
      this->req[this->n++] = c;
      if (this->n < 9)
        continue;
      this->n = 0;
      this->answer_(this->req[2]);
    }
  }

  void answer_(uint8_t cmd) {
    ESP_LOGI(TAG, "probe got request 0x%02X (mode %d)", cmd, this->mode);
    std::vector<uint8_t> f;
    if (cmd == 0x88) {
      f = {0xFF,
           0x88,
           (uint8_t) (this->conc >> 8),
           (uint8_t) this->conc,
           (uint8_t) (this->mode == 5 ? 0x05 : 0x04),
           0x00,
           0x02,
           0x00,
           0};
    } else {
      f = {0xFF, cmd, 0x01, 0, 0, 0, 0, 0, 0};
    }
    f[8] = ck(f);
    switch (this->mode) {
      case 1:
        send(this->u, "probe tail+answer", cat({0x12, 0x34, 0x56}, f));
        break;
      case 2:
        send(this->u, "probe FF tail+answer", cat({0x12, 0xFF, 0x34}, f));
        break;
      case 3:
        ESP_LOGI(TAG, "probe silent");
        break;
      case 4:
        send(this->u, "probe answer head", part(f, 0, 5));
        this->rest = part(f, 5, 9);
        this->rest_at = millis();
        break;
      default:
        send(this->u, "probe answer", f);
    }
  }
};
inline Probe probe;
}  // namespace sim
