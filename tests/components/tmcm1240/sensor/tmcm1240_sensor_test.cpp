#include <gtest/gtest.h>
#include "esphome/components/tmcm1240/sensor/tmcm1240_sensor.h"
#include "../../canopen/test_helpers.h"

namespace esphome::tmcm1240::testing {

TEST(TMCM1240Sensor, SignedThresholdAndWidthRecovery) {
  canopen::testing::MockCANbus bus;
  canopen::testing::ProbeClient client(&bus);
  TMCM1240 motor(&client, 32);
  TMCM1240Sensor sensor(&motor, 0x209E, 1);
  sensor.update(); client.loop();
  client.on_frame_(0x5A0, false, false, {0x4F, 0x9E, 0x20, 0, 0xF8, 0, 0, 0});
  EXPECT_EQ(sensor.state, -8);
  sensor.update(); client.loop();
  client.on_frame_(0x5A0, false, false, {0x4B, 0x9E, 0x20, 0, 8, 0, 0, 0});
  EXPECT_TRUE(std::isnan(sensor.state));
  EXPECT_TRUE(sensor.status_has_warning());
  sensor.update(); client.loop();
  client.on_frame_(0x5A0, false, false, {0x4F, 0x9E, 0x20, 0, 8, 0, 0, 0});
  EXPECT_EQ(sensor.state, 8);
  EXPECT_FALSE(sensor.status_has_warning());
}

}  // namespace esphome::tmcm1240::testing
