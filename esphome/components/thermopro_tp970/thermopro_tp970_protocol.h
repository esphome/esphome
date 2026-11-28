#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome::thermopro_tp970 {

struct TemperatureReading {
  float internal_temperature;
  float ambient_temperature;
  float battery_level;
};

inline float decode_temperature(uint16_t value) { return value == 0xFFFF ? NAN : float(value) - 30.0f; }

inline float decode_battery_level(uint16_t value) {
  if (value == 0xFFFF)
    return NAN;
  // Empirical TP96/TP97 battery curve used by Bluetooth-Devices/thermopro-ble.
  const float level = 52.317286f * std::tanh(float(value) / 273.624277936f - 8.76485439394f) + 51.06925f;
  return std::fmin(100.0f, std::fmax(0.0f, level));
}

inline bool decode_notification(const uint8_t *data, size_t length, TemperatureReading &reading) {
  // TP970 Plus has one probe; do not interpret other message types or probe indexes.
  if (length != 8 || data[0] != 0x10 || data[1] != 0)
    return false;
  const uint16_t internal = uint16_t(data[2]) | (uint16_t(data[3]) << 8);
  const uint16_t battery = uint16_t(data[4]) | (uint16_t(data[5]) << 8);
  const uint16_t ambient = uint16_t(data[6]) | (uint16_t(data[7]) << 8);
  reading = {decode_temperature(internal), decode_temperature(ambient), decode_battery_level(battery)};
  return true;
}

}  // namespace esphome::thermopro_tp970
