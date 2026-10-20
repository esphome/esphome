#include <gtest/gtest.h>
#include "esphome/components/hitachi_ac344/hitachi_ac344.h"

namespace esphome::hitachi_ac344::testing {

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  const remote_base::RemoteTransmitData &data() const { return this->temp_; }

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {}
};

class HitachiSwingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->climate_.set_transmitter(&this->transmitter_);
    this->climate_.mode = climate::CLIMATE_MODE_COOL;
    this->climate_.target_temperature = 24;
    this->climate_.fan_mode = climate::CLIMATE_FAN_AUTO;
    this->climate_.swing_mode = climate::CLIMATE_SWING_OFF;
  }

  // The byte the frame carries at the given position: the header takes two timings and every bit two more.
  uint8_t frame_byte_(size_t index) const {
    const auto &timings = this->transmitter_.data().get_data();
    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (timings[2 + (index * 8 + bit) * 2 + 1] == -static_cast<int32_t>(HITACHI_AC344_ONE_SPACE)) {
        value |= 1 << bit;
      }
    }
    return value;
  }

  uint8_t button_after_swing_(climate::ClimateSwingMode mode) {
    auto call = this->climate_.make_call();
    call.set_swing_mode(mode);
    call.perform();
    return this->frame_byte_(HITACHI_AC344_BUTTON_BYTE);
  }

  HitachiClimate climate_;
  CapturingTransmitter transmitter_;
};

TEST_F(HitachiSwingTest, VerticalSwingIsPressedOnlyWhenItChanges) {
  EXPECT_EQ(this->button_after_swing_(climate::CLIMATE_SWING_VERTICAL), HITACHI_AC344_BUTTON_SWINGV);
  EXPECT_EQ(this->button_after_swing_(climate::CLIMATE_SWING_VERTICAL), HITACHI_AC344_BUTTON_POWER);
  EXPECT_EQ(this->button_after_swing_(climate::CLIMATE_SWING_BOTH), HITACHI_AC344_BUTTON_POWER);
  EXPECT_EQ(this->button_after_swing_(climate::CLIMATE_SWING_HORIZONTAL), HITACHI_AC344_BUTTON_SWINGV);
  EXPECT_EQ(this->button_after_swing_(climate::CLIMATE_SWING_OFF), HITACHI_AC344_BUTTON_POWER);
}

TEST_F(HitachiSwingTest, HorizontalSwingTravelsInTheFrameBits) {
  this->button_after_swing_(climate::CLIMATE_SWING_HORIZONTAL);
  EXPECT_EQ(this->frame_byte_(HITACHI_AC344_SWINGH_BYTE) & 0x07, HITACHI_AC344_SWINGH_AUTO);
  this->button_after_swing_(climate::CLIMATE_SWING_OFF);
  EXPECT_EQ(this->frame_byte_(HITACHI_AC344_SWINGH_BYTE) & 0x07, HITACHI_AC344_SWINGH_MIDDLE);
}

}  // namespace esphome::hitachi_ac344::testing
