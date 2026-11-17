#pragma once
// A Tuya MCU on a second UART, cross-wired to the tuya hub's UART (esphome#22724, 2026-11-14).
// Answers the handshake (heartbeat, product, conf query without status pin so the hub sends WIFI_STATE,
// wifi state, datapoint query), echoes every DATAPOINT_DELIVER back as a report like a real MCU, and plays a
// scripted list of datapoint changes once tuyasim::start() is called. Every frame from the hub is logged as
// `SIM RX #n cmd XX: <hex>` so command streams can be diffed between builds.
// Call tuyasim::poll() from a short interval; set tuyasim::bus in on_boot.
#include <string>
#include <vector>
#include "esphome/components/uart/uart.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace tuyasim {

inline esphome::uart::UARTComponent *bus = nullptr;
inline std::vector<uint8_t> rx;
inline uint32_t frames = 0;
inline uint32_t delivers = 0;
inline bool hb_sent = false;
inline int step = -1;  // -1: scenario not started
inline uint32_t next_step_ms = 0;
inline uint32_t step_gap_ms = 1500;

struct Dp {
  uint8_t id;
  uint8_t type;  // 0 raw, 1 bool, 2 value, 3 string, 4 enum, 5 bitmask
  std::vector<uint8_t> data;
};

inline Dp b(uint8_t id, bool v) { return {id, 1, {static_cast<uint8_t>(v)}}; }
inline Dp v(uint8_t id, int32_t x) {
  uint32_t u = static_cast<uint32_t>(x);
  return {id, 2, {uint8_t(u >> 24), uint8_t(u >> 16), uint8_t(u >> 8), uint8_t(u)}};
}
inline Dp s(uint8_t id, const char *str) { return {id, 3, std::vector<uint8_t>(str, str + strlen(str))}; }
inline Dp e(uint8_t id, uint8_t x) { return {id, 4, {x}}; }
inline Dp m(uint8_t id, std::vector<uint8_t> bytes) { return {id, 5, bytes}; }
inline Dp r(uint8_t id, std::vector<uint8_t> bytes) { return {id, 0, bytes}; }

// Current MCU state, reported in full on DATAPOINT_QUERY
inline std::vector<Dp> &state() {
  static std::vector<Dp> st{b(1, false),        v(2, 100),    s(3, "boot"), e(4, 1),     m(5, {0x01}),
                            r(6, {0x01, 0x02}), b(20, false), v(21, 100),   v(22, 80),   e(23, 2),
                            s(24, "auto"),      v(25, 0),     v(30, 5),     b(31, false)};
  return st;
}

inline void store(const Dp &dp) {
  for (auto &d : state()) {
    if (d.id == dp.id) {
      d = dp;
      return;
    }
  }
  state().push_back(dp);
}

inline void send(uint8_t cmd, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> out{0x55, 0xAA, 0x03, cmd, uint8_t(payload.size() >> 8), uint8_t(payload.size())};
  out.insert(out.end(), payload.begin(), payload.end());
  uint8_t sum = 0;
  for (uint8_t x : out)
    sum += x;
  out.push_back(sum);
  bus->write_array(out.data(), out.size());
}

inline void report(const std::vector<Dp> &dps, uint8_t cmd = 0x07) {
  std::vector<uint8_t> p;
  for (const auto &d : dps) {
    store(d);
    p.push_back(d.id);
    p.push_back(d.type);
    p.push_back(uint8_t(d.data.size() >> 8));
    p.push_back(uint8_t(d.data.size()));
    p.insert(p.end(), d.data.begin(), d.data.end());
  }
  char hex[160];
  esphome::format_hex_to(hex, sizeof(hex), p.data(), p.size() > 70 ? 70 : p.size());
  ESP_LOGI("sim", "SIM TX cmd %02X: %s", cmd, hex);
  send(cmd, p);
}

// The scripted datapoint changes, one entry per step
inline std::vector<std::vector<Dp>> &script() {
  static std::vector<std::vector<Dp>> sc{
      {b(1, true)},
      {v(2, 1234)},
      {s(3, "hello")},
      {e(4, 2)},
      {m(5, {0x01, 0x05})},
      {r(6, {0xDE, 0xAD, 0xBE, 0xEF})},
      {b(20, true), v(21, 120), v(22, 90), e(23, 0)},
      {b(1, false)},  // sent as DATAPOINT_REPORT_SYNC, the hub must ack with 0x23
      {v(2, -5)},
      {s(3, "world 42")},
      {e(4, 0)},
      {s(24, "eco"), v(25, 1)},
      {m(5, {0x80, 0x00, 0x00, 0x01})},
      {e(23, 2), v(22, 100)},
      {v(30, 7), b(31, true)},
      {b(1, true), v(2, 77), s(3, "multi")},
      {v(2, 77)},  // same value again
      {b(20, false)},
  };
  return sc;
}

inline void start() {
  step = 0;
  next_step_ms = esphome::millis();
  ESP_LOGI("sim", "SIM SCENARIO START (%u steps)", (unsigned) script().size());
}

inline void handle(uint8_t cmd, const uint8_t *d, size_t len) {
  frames++;
  char hex[160];
  esphome::format_hex_to(hex, sizeof(hex), d, len > 70 ? 70 : len);
  ESP_LOGI("sim", "SIM RX #%u cmd %02X: %s", (unsigned) frames, cmd, hex);
  switch (cmd) {
    case 0x00:
      send(0x00, {static_cast<uint8_t>(hb_sent ? 0x01 : 0x00)});
      hb_sent = true;
      break;
    case 0x01: {
      const char *p = R"({"p":"tuyasim","v":"1.0.0","m":0})";
      send(0x01, std::vector<uint8_t>(p, p + strlen(p)));
      break;
    }
    case 0x02:
      send(0x02, {});  // no status pin: the hub reports wifi state itself
      break;
    case 0x03:
      send(0x03, {});
      break;
    case 0x08:
      report(state());
      break;
    case 0x06: {
      delivers++;
      // echo each delivered datapoint back as a report, as a real MCU does
      std::vector<Dp> dps;
      size_t i = 0;
      while (i + 4 <= len) {
        size_t n = (d[i + 2] << 8) | d[i + 3];
        if (i + 4 + n > len)
          break;
        dps.push_back({d[i], d[i + 1], std::vector<uint8_t>(d + i + 4, d + i + 4 + n)});
        i += 4 + n;
      }
      ESP_LOGI("sim", "SIM DELIVER #%u (%u dps)", (unsigned) delivers, (unsigned) dps.size());
      report(dps);
      break;
    }
    default:
      break;
  }
}

inline void poll() {
  if (bus == nullptr)
    return;
  uint8_t c;
  while (bus->available() && bus->read_byte(&c)) {
    rx.push_back(c);
    if (rx.size() == 1 && rx[0] != 0x55) {
      rx.clear();
      continue;
    }
    if (rx.size() == 2 && rx[1] != 0xAA) {
      rx.clear();
      continue;
    }
    if (rx.size() < 6)
      continue;
    size_t n = (rx[4] << 8) | rx[5];
    if (rx.size() < 7 + n)
      continue;
    uint8_t sum = 0;
    for (size_t i = 0; i < 6 + n; i++)
      sum += rx[i];
    if (sum != rx[6 + n])
      ESP_LOGW("sim", "SIM RX bad checksum");
    else
      handle(rx[3], rx.data() + 6, n);
    rx.clear();
  }
  if (step >= 0 && step < (int) script().size() && (int32_t) (esphome::millis() - next_step_ms) >= 0) {
    ESP_LOGI("sim", "SIM STEP %d", step);
    report(script()[step], step == 7 ? 0x22 : 0x07);
    step++;
    next_step_ms = esphome::millis() + step_gap_ms;
    if (step == (int) script().size())
      ESP_LOGI("sim", "SIM SCENARIO DONE");
  }
}

}  // namespace tuyasim
