#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/binary_sensor/mk2pvrouter_binary_sensor.h"

namespace esphome::mk2pvrouter::testing {

TEST(Mk2PVRouterBinarySensorTest, ZeroPublishesOff) {
  Mk2PVRouterBinarySensor sensor("R1");
  sensor.publish_val("0");
  ASSERT_TRUE(sensor.has_state());
  EXPECT_FALSE(sensor.state);
}

TEST(Mk2PVRouterBinarySensorTest, OnePublishesOn) {
  Mk2PVRouterBinarySensor sensor("R1");
  sensor.publish_val("1");
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);
}

}  // namespace esphome::mk2pvrouter::testing
