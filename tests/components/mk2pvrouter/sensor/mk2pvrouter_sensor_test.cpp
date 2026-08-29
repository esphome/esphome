#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/sensor/mk2pvrouter_sensor.h"

namespace esphome::mk2pvrouter {

// scale_centi=true divides the parsed value by 100 (e.g. centivolts, centi-degrees).
TEST(Mk2PVRouterSensorTest, ScaleCentiTrueScalesByOneHundredth) {
  Mk2PVRouterSensor sensor("V", true);
  sensor.publish_val("23042");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 230.42f);
}

// scale_centi=false publishes the parsed value unchanged.
TEST(Mk2PVRouterSensorTest, ScaleCentiFalseDoesNotScale) {
  Mk2PVRouterSensor sensor("P1", false);
  sensor.publish_val("1234");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 1234.0f);
}

// An unparseable value is dropped instead of publishing garbage.
TEST(Mk2PVRouterSensorTest, UnparseableValueIsNotPublished) {
  Mk2PVRouterSensor sensor("V1", true);
  sensor.publish_val("not-a-number");
  EXPECT_FALSE(sensor.has_state());
}

}  // namespace esphome::mk2pvrouter
