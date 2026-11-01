#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/sensor/mk2pvrouter_sensor.h"

namespace esphome::mk2pvrouter::testing {

TEST(Mk2PVRouterSensorTest, ScaleCentiTrueScalesByOneHundredth) {
  sensor::Sensor sensor;
  publish_sensor_value(&sensor, "V", true, "V", "23042");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 230.42f);
}

TEST(Mk2PVRouterSensorTest, ScaleCentiFalseDoesNotScale) {
  sensor::Sensor sensor;
  publish_sensor_value(&sensor, "P1", false, "P1", "1234");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 1234.0f);
}

TEST(Mk2PVRouterSensorTest, UnparseableValueIsNotPublished) {
  sensor::Sensor sensor;
  publish_sensor_value(&sensor, "V1", true, "V1", "not-a-number");
  EXPECT_FALSE(sensor.has_state());
}

TEST(Mk2PVRouterSensorTest, OtherTagIsNotPublished) {
  sensor::Sensor sensor;
  publish_sensor_value(&sensor, "P1", false, "P2", "1234");
  EXPECT_FALSE(sensor.has_state());
}

}  // namespace esphome::mk2pvrouter::testing
