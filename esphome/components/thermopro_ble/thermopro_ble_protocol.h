#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome::thermopro_ble {

struct ParseResult {
  float temperature{NAN};
  float external_temperature{NAN};
  float humidity{NAN};
  float battery_level{NAN};
};

inline uint16_t read_u16(const uint8_t *data) { return uint16_t(data[0]) | (uint16_t(data[1]) << 8); }

inline float tp96_temperature(uint16_t raw) { return raw == 0xFFFF ? NAN : float(raw) - 30.0f; }

// Fitted TP96x battery curve, used with permission from Bluetooth-Devices/thermopro-ble.
inline float tp96_battery(uint16_t raw) {
  if (raw == 0xFFFF)
    return NAN;
  const float level = 52.317286f * std::tanh(float(raw) / 273.624277936f - 8.76485439394f) + 51.06925f;
  return std::clamp(level, 0.0f, 100.0f);
}

// The first byte is the probe index; some advertisements append the booster MAC.
inline bool parse_tp96(const uint8_t *data, size_t size, ParseResult &result) {
  if (size != 7 && size != 13)
    return false;
  result.temperature = tp96_temperature(read_u16(data + 1));
  result.battery_level = tp96_battery(read_u16(data + 3));
  result.external_temperature = tp96_temperature(read_u16(data + 5));
  return true;
}

inline bool parse_tp970_notification(const uint8_t *data, size_t size, ParseResult &result) {
  return size == 8 && data[0] == 0x10 && data[1] == 0 && parse_tp96(data + 1, 7, result);
}

class NotificationState {
 public:
  void record(uint32_t now) {
    this->active = true;
    this->last_message = now;
    this->count++;
  }
  void clear() { this->active = false; }
  bool expired(uint32_t now, uint32_t timeout) const {
    return this->active && uint32_t(now - this->last_message) >= timeout;
  }
  bool active{false};
  uint32_t last_message{0};
  uint32_t count{0};
};

}  // namespace esphome::thermopro_ble
