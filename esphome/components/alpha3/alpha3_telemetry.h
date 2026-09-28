#pragma once

#include <array>
#include <cmath>

namespace esphome::alpha3 {

constexpr size_t ALPHA3_TELEMETRY_SENSOR_COUNT = 11;

template<typename SensorType>
void invalidate_sensor_states(const std::array<SensorType *, ALPHA3_TELEMETRY_SENSOR_COUNT> &sensors) {
  for (auto *sensor : sensors) {
    if (sensor != nullptr) {
      sensor->publish_state(NAN);
    }
  }
}

}  // namespace esphome::alpha3
