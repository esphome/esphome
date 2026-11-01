// esphbot review finding 2: RS485FrameSensor::loop() (pre-fix) published on every pass
// where the decoded value differed from the last publish, with no polling gate -- fine for
// a counter that only changes occasionally, but frames_received changes on nearly every RX
// frame (10-100/s on a live bus), flooding the API connection and the HA recorder. The fix
// makes RS485FrameSensor a PollingComponent; update() must keep the existing
// publish-only-on-change behavior, now gated to run once per update_interval: instead of
// once per main loop iteration.

#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/rs485_frame/rs485_frame.h"
#include "esphome/components/rs485_frame/sensor/rs485_frame_sensor.h"

namespace esphome::rs485_frame::testing {

namespace {

// Exposes a setter for the protected frames_received_ counter so the test can drive
// RS485FrameSensor::update() without standing up a real UART/RX path.
class RS485FrameHubProbe : public RS485FrameHub {
 public:
  void set_frames_received_for_test(uint32_t value) { this->frames_received_ = value; }
};

}  // namespace

TEST(RS485FrameSensorTest, UpdatePublishesOnlyWhenTheDecodedValueChanges) {
  RS485FrameHubProbe hub;
  hub.set_frames_received_for_test(5);

  RS485FrameSensor sens;
  sens.set_parent(&hub);
  sens.set_decode(SENSOR_DECODE_FRAMES_RECEIVED);

  int publish_count = 0;
  sens.add_on_state_callback([&](float) { publish_count++; });

  sens.update();
  EXPECT_EQ(publish_count, 1);
  EXPECT_EQ(sens.get_state(), 5.0f);

  // Same value again: must not publish a second time.
  sens.update();
  EXPECT_EQ(publish_count, 1);

  // Value changed: must publish exactly once more, with the new value.
  hub.set_frames_received_for_test(6);
  sens.update();
  EXPECT_EQ(publish_count, 2);
  EXPECT_EQ(sens.get_state(), 6.0f);
}

}  // namespace esphome::rs485_frame::testing
