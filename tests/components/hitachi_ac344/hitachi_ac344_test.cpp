#include <gtest/gtest.h>
#include <array>
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

  // Feed the unit's own frame to the receiver: header, 43 bytes least significant bit first, footer mark.
  void receive_(const std::array<uint8_t, HITACHI_AC344_STATE_LENGTH> &frame) {
    remote_base::RawTimings timings{HITACHI_AC344_HDR_MARK, -static_cast<int32_t>(HITACHI_AC344_HDR_SPACE)};
    for (uint8_t byte : frame) {
      for (uint8_t bit = 0; bit < 8; bit++) {
        timings.push_back(HITACHI_AC344_BIT_MARK);
        timings.push_back(
            -static_cast<int32_t>(byte & (1 << bit) ? HITACHI_AC344_ONE_SPACE : HITACHI_AC344_ZERO_SPACE));
      }
    }
    timings.push_back(HITACHI_AC344_BIT_MARK);
    remote_base::RemoteReceiveData data(timings, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(this->climate_).on_receive(data));
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

TEST_F(HitachiSwingTest, ReceivedFrameResyncsTheVerticalSwingState) {
  // A cooling frame from the remote with vertical swing on: power on, mode cool, swing bit set.
  std::array<uint8_t, HITACHI_AC344_STATE_LENGTH> frame{};
  frame[HITACHI_AC344_POWER_BYTE] = HITACHI_AC344_POWER_ON;
  frame[HITACHI_AC344_MODE_BYTE] = HITACHI_AC344_MODE_COOL | (HITACHI_AC344_FAN_AUTO << 4);
  frame[HITACHI_AC344_TEMP_BYTE] = 24 << HITACHI_AC344_TEMP_OFFSET;
  frame[HITACHI_AC344_SWINGH_BYTE] = HITACHI_AC344_SWINGH_MIDDLE;
  frame[HITACHI_AC344_SWINGV_BYTE] = 1 << HITACHI_AC344_SWINGV_OFFSET;
  // An on_state automation that transmits from the publish inside on_receive must already see the received frame,
  // so its temperature change goes out as a power press and not another swing press.
  bool transmitted = false;
  uint8_t button_from_callback = 0;
  this->climate_.add_on_state_callback([&](climate::Climate &) {
    if (transmitted)
      return;
    transmitted = true;
    auto call = this->climate_.make_call();
    call.set_target_temperature(22);
    call.perform();
    button_from_callback = this->frame_byte_(HITACHI_AC344_BUTTON_BYTE);
  });
  this->receive_(frame);
  EXPECT_EQ(this->climate_.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  EXPECT_TRUE(transmitted);
  EXPECT_EQ(button_from_callback, HITACHI_AC344_BUTTON_POWER);
  EXPECT_EQ(this->button_after_swing_(climate::CLIMATE_SWING_OFF), HITACHI_AC344_BUTTON_SWINGV);
}

TEST_F(HitachiSwingTest, RestoredVerticalSwingSeedsTheTrackedState) {
  static_cast<Component &>(this->climate_).setup();
  EXPECT_EQ(this->button_after_swing_(climate::CLIMATE_SWING_VERTICAL), HITACHI_AC344_BUTTON_SWINGV);

  // After a reboot the unit still swings vertically, so a temperature change must not press the swing button.
  HitachiClimate rebooted;
  rebooted.set_transmitter(&this->transmitter_);
  static_cast<Component &>(rebooted).setup();
  EXPECT_EQ(rebooted.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  auto call = rebooted.make_call();
  call.set_target_temperature(22);
  call.perform();
  EXPECT_EQ(this->frame_byte_(HITACHI_AC344_BUTTON_BYTE), HITACHI_AC344_BUTTON_POWER);
}

}  // namespace esphome::hitachi_ac344::testing
