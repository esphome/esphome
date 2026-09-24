#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "esphome/components/alpha3/alpha3_telemetry.h"

namespace esphome::alpha3::testing {

class TelemetrySensor {
 public:
  void publish_state(float state) { this->state = state; }

  float state{NAN};
};

TEST(Alpha3Telemetry, InvalidatesEveryConfiguredNumericSensor) {
  std::array<TelemetrySensor, ALPHA3_TELEMETRY_SENSOR_COUNT> sensors;
  std::array<TelemetrySensor *, ALPHA3_TELEMETRY_SENSOR_COUNT> configured{};
  for (size_t index = 0; index < sensors.size(); index++) {
    sensors[index].publish_state(static_cast<float>(index));
    configured[index] = &sensors[index];
  }

  invalidate_sensor_states(configured);

  for (const auto &sensor : sensors) {
    EXPECT_TRUE(std::isnan(sensor.state));
  }
}

TEST(Alpha3Telemetry, IgnoresUnconfiguredSensors) {
  TelemetrySensor configured_sensor;
  configured_sensor.publish_state(1.0F);
  std::array<TelemetrySensor *, ALPHA3_TELEMETRY_SENSOR_COUNT> sensors{};
  sensors[3] = &configured_sensor;

  invalidate_sensor_states(sensors);

  EXPECT_TRUE(std::isnan(configured_sensor.state));
}

}  // namespace esphome::alpha3::testing
