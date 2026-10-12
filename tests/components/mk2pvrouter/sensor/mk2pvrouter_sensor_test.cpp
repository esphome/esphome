#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/sensor/mk2pvrouter_sensor.h"

namespace esphome::mk2pvrouter::testing {

TEST(Mk2PVRouterSensorTest, ScaleCentiTrueScalesByOneHundredth) {
  Mk2PVRouterSensor sensor("V", true);
  sensor.publish_val("23042");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 230.42f);
}

TEST(Mk2PVRouterSensorTest, ScaleCentiFalseDoesNotScale) {
  Mk2PVRouterSensor sensor("P1", false);
  sensor.publish_val("1234");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 1234.0f);
}

TEST(Mk2PVRouterSensorTest, UnparseableValueIsNotPublished) {
  Mk2PVRouterSensor sensor("V1", true);
  sensor.publish_val("not-a-number");
  EXPECT_FALSE(sensor.has_state());
}

}  // namespace esphome::mk2pvrouter::testing
