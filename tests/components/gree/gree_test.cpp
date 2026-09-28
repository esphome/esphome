#include <gtest/gtest.h>
#include "esphome/components/gree/gree.h"

namespace esphome::gree::testing {

namespace {

class CaptureTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CaptureTransmitter() : RemoteTransmitterBase(nullptr) {}
  remote_base::RawTimings sent;

 protected:
  void send_internal(uint32_t /*send_times*/, uint32_t /*send_wait*/) override { this->sent = this->temp_.get_data(); }
};

remote_base::RawTimings transmit(Model model, climate::ClimateSwingMode swing = climate::CLIMATE_SWING_OFF) {
  GreeClimate climate;
  CaptureTransmitter transmitter;
  climate.set_transmitter(&transmitter);
  climate.set_model(model);
  climate.make_call()
      .set_mode(climate::CLIMATE_MODE_COOL)
      .set_target_temperature(24.0f)
      .set_fan_mode(climate::CLIMATE_FAN_LOW)
      .set_swing_mode(swing)
      .perform();
  return transmitter.sent;
}

// Reads `count` bytes, LSB first, from the bit pairs starting at `index`
std::vector<uint8_t> read_bytes(const remote_base::RawTimings &timings, size_t index, size_t count) {
  std::vector<uint8_t> bytes(count, 0);
  for (size_t bit = 0; bit < count * 8; bit++) {
    if (-timings[index + bit * 2 + 1] == GREE_ONE_SPACE)
      bytes[bit / 8] |= 1 << (bit % 8);
  }
  return bytes;
}

// An 8-byte frame is header (2) + 32 bits (64) + 010 connector (6) + gap (2) + 32 bits (64)
constexpr size_t FRAME_LENGTH = 2 + 64 + 6 + 2 + 64;

std::vector<uint8_t> read_frame(const remote_base::RawTimings &timings, size_t start) {
  auto bytes = read_bytes(timings, start + 2, 4);
  auto second = read_bytes(timings, start + 2 + 64 + 6 + 2, 4);
  bytes.insert(bytes.end(), second.begin(), second.end());
  return bytes;
}

}  // namespace

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

TEST(GreeClimateTest, YacSendsOneEightByteFrame) {
  auto timings = transmit(GREE_YAC);
  ASSERT_EQ(timings.size(), FRAME_LENGTH + 2);
  EXPECT_EQ(timings[0], GREE_HEADER_MARK);
  EXPECT_EQ(timings[1], -(int32_t) GREE_HEADER_SPACE);
  EXPECT_EQ(timings[73], -(int32_t) GREE_MESSAGE_SPACE);
  // Without a light switch the mode bits are clear, so byte 2 is 0x00
  EXPECT_EQ(read_frame(timings, 0), (std::vector<uint8_t>{0x19, 0x08, 0x00, 0x50, 0x00, 0x20, 0x20, 0xF0}));
}

TEST(GreeClimateTest, Yac16SendsTwoEightByteFrames) {
  auto timings = transmit(GREE_YAC16);
  ASSERT_EQ(timings.size(), FRAME_LENGTH + 3 + FRAME_LENGTH + 2);

  EXPECT_EQ(read_frame(timings, 0), (std::vector<uint8_t>{0x19, 0x08, 0x20, 0x50, 0x00, 0xC0, 0x20, 0x90}));
  EXPECT_EQ(read_frame(timings, FRAME_LENGTH + 3),
            (std::vector<uint8_t>{0x00, 0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0xA0}));

  for (size_t start : {(size_t) 0, FRAME_LENGTH + 3}) {
    EXPECT_EQ(timings[start], GREE_HEADER_MARK);
    EXPECT_EQ(timings[start + 1], -(int32_t) GREE_YAC16_HEADER_SPACE);
    EXPECT_EQ(timings[start + 2], GREE_YAC16_BIT_MARK);
    EXPECT_EQ(timings[start + 2 + 64 + 6 + 1], -(int32_t) GREE_YAC16_MESSAGE_SPACE);
  }
  // The frames are separated by two message spaces
  EXPECT_EQ(timings[FRAME_LENGTH + 1], -(int32_t) GREE_YAC16_MESSAGE_SPACE);
  EXPECT_EQ(timings[FRAME_LENGTH + 2], -(int32_t) GREE_YAC16_MESSAGE_SPACE);
  EXPECT_EQ(timings.back(), 0);
}

TEST(GreeClimateTest, Yac16HorizontalSwingIsInTheChecksum) {
  auto timings = transmit(GREE_YAC16, climate::CLIMATE_SWING_HORIZONTAL);
  ASSERT_EQ(timings.size(), FRAME_LENGTH + 3 + FRAME_LENGTH + 2);
  EXPECT_EQ(read_frame(timings, 0), (std::vector<uint8_t>{0x19, 0x08, 0x20, 0x50, 0x10, 0xC0, 0x20, 0xA0}));
}

}  // namespace esphome::gree::testing
