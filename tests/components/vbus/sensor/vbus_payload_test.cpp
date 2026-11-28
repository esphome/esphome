#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/vbus/sensor/vbus_sensor.h"

namespace esphome::vbus::testing {
namespace {

TEST(VBusSensorTest, RejectsShortPayloadAndPublishesNextCompletePayload) {
  DeltaSolCS2Sensor listener;
  sensor::Sensor pump_speed;
  listener.set_pump_speed_sensor(&pump_speed);

  std::vector<uint8_t> payload(28);
  payload[12] = 42;
  listener.on_message(0x0100, 0x1121, 0x0010, payload);
  EXPECT_FALSE(pump_speed.has_state());

  payload.resize(32);
  listener.on_message(0x0100, 0x1121, 0x0010, payload);
  ASSERT_TRUE(pump_speed.has_state());
  EXPECT_FLOAT_EQ(pump_speed.state, 42.0f);
}

}  // namespace
}  // namespace esphome::vbus::testing
