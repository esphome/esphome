#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/sensor/mk2pvrouter_sensor.h"

namespace esphome::mk2pvrouter {

// Bare "V" is the exact-match voltage tag; the device sends it multiplied by 100.
TEST(Mk2PVRouterSensorTest, BareVIsScaledByOneHundredth) {
  Mk2PVRouterSensor sensor("V");
  sensor.publish_val("23042");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 230.42f);
}

// "V" followed by digits (per-phase voltage) is scaled the same way as the bare tag.
TEST(Mk2PVRouterSensorTest, IndexedVIsScaledByOneHundredth) {
  Mk2PVRouterSensor sensor("V1");
  sensor.publish_val("23042");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 230.42f);
}

// "T" followed by digits (per-probe temperature) is scaled; bare "T" never occurs on the wire.
TEST(Mk2PVRouterSensorTest, IndexedTIsScaledByOneHundredth) {
  Mk2PVRouterSensor sensor("T1");
  sensor.publish_val("2350");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 23.5f);
}

// Bare "T" is not a real tag on the wire, so it must not be mistaken for the indexed, scaled form.
TEST(Mk2PVRouterSensorTest, BareTIsNotScaled) {
  Mk2PVRouterSensor sensor("T");
  sensor.publish_val("2350");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 2350.0f);
}

// Tags outside the V/T scaling rule (e.g. power) are published as-is.
TEST(Mk2PVRouterSensorTest, UnrelatedTagIsNotScaled) {
  Mk2PVRouterSensor sensor("P1");
  sensor.publish_val("1234");
  EXPECT_FLOAT_EQ(sensor.get_raw_state(), 1234.0f);
}

// An unparseable value is dropped instead of publishing garbage.
TEST(Mk2PVRouterSensorTest, UnparseableValueIsNotPublished) {
  Mk2PVRouterSensor sensor("V1");
  sensor.publish_val("not-a-number");
  EXPECT_FALSE(sensor.has_state());
}

}  // namespace esphome::mk2pvrouter
