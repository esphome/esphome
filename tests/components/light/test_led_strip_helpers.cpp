#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"
#include "esphome/components/light/light_state.h"

namespace esphome::light::testing {

namespace {

constexpr ChannelColors GRB{1, 0, 2, ChannelColors::NO_WHITE};
constexpr ChannelColors GRBX{1, 0, 2, ChannelColors::RESERVED_WHITE};
constexpr ChannelColors GRBW{1, 0, 2, 3};

// Exposes the protected strip helpers
class HelperStrip : public AddressableLight {
 public:
  int32_t size() const override { return 1; }
  void clear_effect_data() override {}
  LightTraits get_traits() override { return {}; }
  void write_state(LightState *state) override {}
  using AddressableLight::throttle_show_;
};

}  // namespace

TEST(LedStripHelpers, HasWhite) {
  EXPECT_FALSE(GRB.has_white());
  EXPECT_FALSE(GRBX.has_white());
  EXPECT_TRUE(GRBW.has_white());
}

TEST(LedStripHelpers, BytesPerLed) {
  EXPECT_EQ(3, GRB.bytes_per_led());
  EXPECT_EQ(4, GRBX.bytes_per_led());
  EXPECT_EQ(4, GRBW.bytes_per_led());
}

TEST(LedStripHelpers, ToString) {
  char buf[5];
  EXPECT_STREQ("GRB", GRB.to_string(buf));
  EXPECT_STREQ("GRBX", GRBX.to_string(buf));
  EXPECT_STREQ("GRBW", GRBW.to_string(buf));
}

TEST(LedStripHelpers, TraitsFollowTheWhiteChannel) {
  const auto rgb = GRB.traits().get_supported_color_modes();
  EXPECT_TRUE(rgb.count(ColorMode::RGB));
  EXPECT_EQ(rgb.size(), 1u);
  const auto rgbw = GRBW.traits().get_supported_color_modes();
  EXPECT_TRUE(rgbw.count(ColorMode::RGB_WHITE));
  EXPECT_TRUE(rgbw.count(ColorMode::WHITE));
  EXPECT_EQ(rgbw.size(), 2u);
}

TEST(LedStripHelpers, GetSetRGBColors) {
  std::array<uint8_t, 9> buf{};
  GRB.set(buf.data(), 1, Color(10, 20, 30, 40));
  EXPECT_EQ(buf, (std::array<uint8_t, 9>{0, 0, 0, 20, 10, 30, 0, 0, 0}));
  EXPECT_EQ(Color(10, 20, 30, 0).raw_32, GRB.get(buf.data(), 1).raw_32);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_R), 10);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_G), 20);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_B), 30);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_W), 0);
  GRB.set_channel(buf.data(), 1, COLOR_CHANNEL_R, 11);
  GRB.set_channel(buf.data(), 1, COLOR_CHANNEL_G, 21);
  GRB.set_channel(buf.data(), 1, COLOR_CHANNEL_B, 31);
  GRB.set_channel(buf.data(), 1, COLOR_CHANNEL_W, 41);
  EXPECT_EQ(Color(11, 21, 31, 0).raw_32, GRB.get(buf.data(), 1).raw_32);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_R), 11);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_G), 21);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_B), 31);
  EXPECT_EQ(GRB.get_channel(buf.data(), 1, COLOR_CHANNEL_W), 0);
  EXPECT_EQ(buf, (std::array<uint8_t, 9>{0, 0, 0, 21, 11, 31, 0, 0, 0}));
}

TEST(LedStripHelpers, GetSetRGBXColors) {
  std::array<uint8_t, 12> buf{};
  GRBX.set(buf.data(), 1, Color(10, 20, 30, 40));
  EXPECT_EQ(buf, (std::array<uint8_t, 12>{0, 0, 0, 0, 20, 10, 30, 0, 0, 0, 0, 0}));
  EXPECT_EQ(Color(10, 20, 30, 0).raw_32, GRBX.get(buf.data(), 1).raw_32);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_R), 10);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_G), 20);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_B), 30);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_W), 0);
  GRBX.set_channel(buf.data(), 1, COLOR_CHANNEL_R, 11);
  GRBX.set_channel(buf.data(), 1, COLOR_CHANNEL_G, 21);
  GRBX.set_channel(buf.data(), 1, COLOR_CHANNEL_B, 31);
  GRBX.set_channel(buf.data(), 1, COLOR_CHANNEL_W, 41);
  EXPECT_EQ(Color(11, 21, 31, 0).raw_32, GRBX.get(buf.data(), 1).raw_32);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_R), 11);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_G), 21);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_B), 31);
  EXPECT_EQ(GRBX.get_channel(buf.data(), 1, COLOR_CHANNEL_W), 0);
  EXPECT_EQ(buf, (std::array<uint8_t, 12>{0, 0, 0, 0, 21, 11, 31, 0, 0, 0, 0, 0}));
}

TEST(LedStripHelpers, GetSetRGBWColors) {
  std::array<uint8_t, 12> buf{};
  GRBW.set(buf.data(), 1, Color(10, 20, 30, 40));
  EXPECT_EQ(buf, (std::array<uint8_t, 12>{0, 0, 0, 0, 20, 10, 30, 40, 0, 0, 0, 0}));
  EXPECT_EQ(Color(10, 20, 30, 40).raw_32, GRBW.get(buf.data(), 1).raw_32);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_R), 10);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_G), 20);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_B), 30);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_W), 40);
  GRBW.set_channel(buf.data(), 1, COLOR_CHANNEL_R, 11);
  GRBW.set_channel(buf.data(), 1, COLOR_CHANNEL_G, 21);
  GRBW.set_channel(buf.data(), 1, COLOR_CHANNEL_B, 31);
  GRBW.set_channel(buf.data(), 1, COLOR_CHANNEL_W, 41);
  EXPECT_EQ(Color(11, 21, 31, 41).raw_32, GRBW.get(buf.data(), 1).raw_32);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_R), 11);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_G), 21);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_B), 31);
  EXPECT_EQ(GRBW.get_channel(buf.data(), 1, COLOR_CHANNEL_W), 41);
  EXPECT_EQ(buf, (std::array<uint8_t, 12>{0, 0, 0, 0, 21, 11, 31, 41, 0, 0, 0, 0}));
}

TEST(LedStripHelpers, ThrottleShowHoldsTheFrame) {
  HelperStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  EXPECT_FALSE(strip.throttle_show_(1000, 0, 0));
  EXPECT_TRUE(strip.throttle_show_(1000, 500, 600));
  EXPECT_FALSE(strip.throttle_show_(1100, 500, 600));
  // micros() wraps
  EXPECT_TRUE(strip.throttle_show_(100, UINT32_MAX - 99, 300));
  EXPECT_FALSE(strip.throttle_show_(200, UINT32_MAX - 99, 300));
}

}  // namespace esphome::light::testing
