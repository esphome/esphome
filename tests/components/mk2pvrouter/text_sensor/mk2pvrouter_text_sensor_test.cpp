#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/text_sensor/mk2pvrouter_text_sensor.h"

namespace esphome::mk2pvrouter::testing {

TEST(Mk2PVRouterTextSensorTest, PublishesValueVerbatim) {
  Mk2PVRouterTextSensor sensor("S_MC");
  sensor.publish_val("-1234");
  EXPECT_EQ(sensor.get_state(), "-1234");
}

}  // namespace esphome::mk2pvrouter::testing
