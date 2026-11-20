#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/text_sensor/mk2pvrouter_text_sensor.h"

namespace esphome::mk2pvrouter::testing {

TEST(Mk2PVRouterTextSensorTest, PublishesValueVerbatim) {
  text_sensor::TextSensor sensor;
  publish_text_sensor_value(&sensor, "S_MC", "S_MC", "-1234");
  ASSERT_TRUE(sensor.has_state());
  EXPECT_EQ(sensor.get_state(), "-1234");
}

TEST(Mk2PVRouterTextSensorTest, ShorterValueReplacesPreviousOne) {
  text_sensor::TextSensor sensor;
  publish_text_sensor_value(&sensor, "S_MC", "S_MC", "-1234");
  publish_text_sensor_value(&sensor, "S_MC", "S_MC", "5");
  EXPECT_EQ(sensor.get_state(), "5");
}

}  // namespace esphome::mk2pvrouter::testing
