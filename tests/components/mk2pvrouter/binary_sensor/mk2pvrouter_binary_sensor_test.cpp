#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/binary_sensor/mk2pvrouter_binary_sensor.h"

namespace esphome::mk2pvrouter::testing {

TEST(Mk2PVRouterBinarySensorTest, ZeroPublishesOff) {
  binary_sensor::BinarySensor sensor;
  publish_binary_sensor_value(&sensor, "R1", "R1", "0");
  ASSERT_TRUE(sensor.has_state());
  EXPECT_FALSE(sensor.state);
}

TEST(Mk2PVRouterBinarySensorTest, OnePublishesOn) {
  binary_sensor::BinarySensor sensor;
  publish_binary_sensor_value(&sensor, "R1", "R1", "1");
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);
}

}  // namespace esphome::mk2pvrouter::testing
