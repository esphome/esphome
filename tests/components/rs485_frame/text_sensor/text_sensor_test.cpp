// RS485FrameTextSensor publishes the hub's last validated frame type. On a live bus that value
// changes on nearly every frame (Hayward interleaves keep-alive, LED, and display frames; Jandy
// polls several devices in turn), so a loop()-driven change filter still publishes per frame.
// It is a PollingComponent: update() publishes only when the value differs from the last
// publish, once per update_interval.

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "esphome/components/rs485_frame/rs485_frame.h"
#include "esphome/components/rs485_frame/text_sensor/rs485_frame_text_sensor.h"

namespace esphome::rs485_frame::testing {

namespace {

// Exposes a setter for the protected last_frame_type_ buffer so the test can drive
// RS485FrameTextSensor::update() without standing up a real UART/RX path.
class RS485FrameHubProbe : public RS485FrameHub {
 public:
  void set_last_frame_type_for_test(const char *value) {
    std::strncpy(this->last_frame_type_, value, sizeof(this->last_frame_type_) - 1);
    this->last_frame_type_[sizeof(this->last_frame_type_) - 1] = '\0';
  }
};

}  // namespace

TEST(RS485FrameTextSensorTest, UpdatePublishesOnlyWhenTheFrameTypeChanges) {
  RS485FrameHubProbe hub;

  RS485FrameTextSensor sens;
  sens.set_parent(&hub);

  int publish_count = 0;
  sens.add_on_state_callback([&](const std::string &) { publish_count++; });

  // No frame received yet: nothing to publish.
  sens.update();
  EXPECT_EQ(publish_count, 0);

  hub.set_last_frame_type_for_test("0101");
  sens.update();
  EXPECT_EQ(publish_count, 1);
  EXPECT_EQ(sens.get_state(), "0101");

  // Same value again: must not publish a second time.
  sens.update();
  EXPECT_EQ(publish_count, 1);

  // Value changed: must publish exactly once more, with the new value.
  hub.set_last_frame_type_for_test("0102");
  sens.update();
  EXPECT_EQ(publish_count, 2);
  EXPECT_EQ(sens.get_state(), "0102");
}

}  // namespace esphome::rs485_frame::testing
