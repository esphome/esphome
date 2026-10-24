#include <gtest/gtest.h>
#include <array>
#include "esphome/components/gree/gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::gree::testing {

using Frame = std::array<uint8_t, GREE_STATE_FRAME_SIZE>;

class TestGreeClimate : public GreeClimate {
 public:
  using GreeClimate::on_receive;
  uint8_t mode_bits() const { return this->mode_bits_; }
};

// Builds the timings a remote sends for `frame`, using the same layout as transmit_state().
static remote_base::RawTimings timings_for(const Frame &frame, uint32_t header_space = GREE_HEADER_SPACE,
                                           uint32_t message_space = GREE_MESSAGE_SPACE) {
  remote_base::RawTimings raw;
  auto mark = [&raw](uint32_t length) { raw.push_back(static_cast<int32_t>(length)); };
  auto space = [&raw](uint32_t length) { raw.push_back(-static_cast<int32_t>(length)); };
  auto push_byte = [&](uint8_t value) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      mark(GREE_BIT_MARK);
      space(((value >> bit) & 1) ? GREE_ONE_SPACE : GREE_ZERO_SPACE);
    }
  };

  mark(GREE_HEADER_MARK);
  space(header_space);
  for (size_t i = 0; i < 4; i++) {
    push_byte(frame[i]);
  }
  mark(GREE_BIT_MARK);
  space(GREE_ZERO_SPACE);
  mark(GREE_BIT_MARK);
  space(GREE_ONE_SPACE);
  mark(GREE_BIT_MARK);
  space(GREE_ZERO_SPACE);
  mark(GREE_BIT_MARK);
  space(message_space);
  for (size_t i = 4; i < GREE_STATE_FRAME_SIZE; i++) {
    push_byte(frame[i]);
  }
  mark(GREE_BIT_MARK);
  return raw;
}

static bool receive(TestGreeClimate &climate, const remote_base::RawTimings &raw) {
  return climate.on_receive(remote_base::RemoteReceiveData(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
}

// Golden frames. Byte 7 is the checksum, computed independently of the decoder.
// Generic, YAC1FB9 and YAG: nibble sum. YAN and YX1FF: (byte0 << 4) + (byte1 << 4) + 0xC0.
static constexpr Frame GENERIC_COOL_FAN1 = {0x19, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0xD0};
static constexpr Frame YAN_COOL_VANE_UP = {0x09, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0xD0};
static constexpr Frame YAN_COOL_VANE_SWING = {0x09, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xD0};
static constexpr Frame YAN_COOL_VANE_UNKNOWN = {0x09, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00, 0xD0};
static constexpr Frame YAN_COOL_MODE_BITS = {0x09, 0x08, 0x30, 0x00, 0x02, 0x00, 0x00, 0xD0};
static constexpr Frame YAG_COOL_SWING_BOTH = {0x09, 0x08, 0x00, 0x00, 0x11, 0x20, 0x00, 0xE0};
static constexpr Frame X1FF_COOL_SLEEP = {0x89, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0xD0};
static constexpr Frame X1FF_COOL_TURBO = {0x89, 0x08, 0x10, 0x00, 0x02, 0x00, 0x00, 0xD0};
static constexpr Frame YAC1FB9_COOL_SLEEP = {0x89, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0};

TEST(GreeClimateTest, HeatCoolHiddenWithoutHeatByDefault) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  EXPECT_FALSE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(GreeClimateTest, HeatCoolOverrideAdvertisedWithoutHeat) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
}

TEST(GreeClimateTest, GenericFrameLeavesSwingAlone) {
  TestGreeClimate climate;
  climate.set_model(GREE_GENERIC);
  climate.swing_mode = climate::CLIMATE_SWING_HORIZONTAL;

  ASSERT_TRUE(receive(climate, timings_for(GENERIC_COOL_FAN1)));
  EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(climate.target_temperature, 24.0f);
  EXPECT_EQ(climate.fan_mode, climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(climate.swing_mode, climate::CLIMATE_SWING_HORIZONTAL);
  EXPECT_EQ(climate.mode_bits(), GREE_MODE_BIT_LIGHT);
}

TEST(GreeClimateTest, YanVaneSwingDecodesVertical) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAN);

  ASSERT_TRUE(receive(climate, timings_for(YAN_COOL_VANE_SWING)));
  EXPECT_EQ(climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST(GreeClimateTest, YanFixedVaneDecodesSwingOff) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAN);
  climate.swing_mode = climate::CLIMATE_SWING_VERTICAL;

  ASSERT_TRUE(receive(climate, timings_for(YAN_COOL_VANE_UP)));
  EXPECT_EQ(climate.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(GreeClimateTest, YanModeBitsAreTakenFromFrame) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAN);

  ASSERT_TRUE(receive(climate, timings_for(YAN_COOL_MODE_BITS)));
  EXPECT_EQ(climate.mode_bits(), 0x30);
}

TEST(GreeClimateTest, YagFrameDecodesBothSwingAxes) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAG);

  ASSERT_TRUE(receive(climate, timings_for(YAG_COOL_SWING_BOTH)));
  EXPECT_EQ(climate.swing_mode, climate::CLIMATE_SWING_BOTH);
  EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_COOL);
}

TEST(GreeClimateTest, X1ffSleepBitDecodesSleepPreset) {
  TestGreeClimate climate;
  climate.set_model(GREE_YX1FF);

  ASSERT_TRUE(receive(climate, timings_for(X1FF_COOL_SLEEP)));
  EXPECT_EQ(climate.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_EQ(climate.fan_mode, climate::CLIMATE_FAN_AUTO);
}

TEST(GreeClimateTest, X1ffTurboBitIsFanNotSleep) {
  TestGreeClimate climate;
  climate.set_model(GREE_YX1FF);

  // Turbo also sets byte 0 bit 7, so the frame must not read as sleep.
  ASSERT_TRUE(receive(climate, timings_for(X1FF_COOL_TURBO)));
  EXPECT_EQ(climate.fan_mode, climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(climate.preset, climate::CLIMATE_PRESET_NONE);
}

TEST(GreeClimateTest, Yac1fb9SleepFrameUsesItsOwnTiming) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAC1FB9);

  ASSERT_TRUE(receive(climate, timings_for(YAC1FB9_COOL_SLEEP, GREE_YAC1FB9_HEADER_SPACE, GREE_YAC1FB9_MESSAGE_SPACE)));
  EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(climate.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_EQ(climate.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(GreeClimateTest, BadChecksumIsRejectedWithoutChangingState) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAN);
  climate.mode = climate::CLIMATE_MODE_DRY;
  Frame frame = YAN_COOL_VANE_UP;
  frame[7] = 0x00;

  EXPECT_FALSE(receive(climate, timings_for(frame)));
  EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_DRY);
}

TEST(GreeClimateTest, UnknownVaneValueIsRejectedWithoutChangingState) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAN);
  climate.mode = climate::CLIMATE_MODE_DRY;
  climate.swing_mode = climate::CLIMATE_SWING_OFF;

  // The checksum is still valid, so only the vane check can reject this frame.
  EXPECT_FALSE(receive(climate, timings_for(YAN_COOL_VANE_UNKNOWN)));
  EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_DRY);
  EXPECT_EQ(climate.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(GreeClimateTest, IncompleteFrameIsRejectedWithoutChangingState) {
  TestGreeClimate climate;
  climate.set_model(GREE_YAN);
  climate.mode = climate::CLIMATE_MODE_DRY;

  auto raw = timings_for(YAN_COOL_VANE_UP);
  raw.pop_back();  // Drop the trailing mark, so the frame never completes

  EXPECT_FALSE(receive(climate, raw));
  EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_DRY);
}

}  // namespace esphome::gree::testing
