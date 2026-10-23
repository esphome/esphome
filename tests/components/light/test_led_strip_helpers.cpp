#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"
#include "esphome/components/light/light_state.h"

namespace esphome::light::testing {

namespace {

constexpr ChannelColors GRB{1, 0, 2, ChannelColors::NO_WHITE};
constexpr ChannelColors GRBW{1, 0, 2, 3};

// Exposes the protected strip helpers
class HelperStrip : public AddressableLight {
 public:
  int32_t size() const override { return 1; }
  void clear_effect_data() override {}
  LightTraits get_traits() override { return {}; }
  void write_state(LightState *state) override {}
  using AddressableLight::refresh_too_soon_;
  using AddressableLight::zero_effect_data_;

 protected:
  ESPColorView get_view_internal(int32_t index) const override {
    return GRB.view(nullptr, nullptr, index, &this->correction_);
  }
};

}  // namespace

TEST(LedStripHelpers, TraitsFollowTheWhiteChannel) {
  const auto rgb = GRB.traits().get_supported_color_modes();
  EXPECT_TRUE(rgb.count(ColorMode::RGB));
  EXPECT_EQ(rgb.size(), 1u);
  const auto rgbw = GRBW.traits().get_supported_color_modes();
  EXPECT_TRUE(rgbw.count(ColorMode::RGB_WHITE));
  EXPECT_TRUE(rgbw.count(ColorMode::WHITE));
  EXPECT_EQ(rgbw.size(), 2u);
}

TEST(LedStripHelpers, ViewWritesTheChannelOrder) {
  ESPColorCorrection correction;
  std::array<uint8_t, 8> buf{};
  std::array<uint8_t, 2> effect{};
  GRBW.view(buf.data(), effect.data(), 1, &correction) = Color(10, 20, 30, 40);
  GRBW.view(buf.data(), effect.data(), 1, &correction).set_effect_data(7);
  EXPECT_EQ(buf, (std::array<uint8_t, 8>{0, 0, 0, 0, 20, 10, 30, 40}));
  EXPECT_EQ(effect[1], 7);

  std::array<uint8_t, 6> rgb_buf{};
  auto view = GRB.view(rgb_buf.data(), effect.data(), 1, &correction);
  view = Color(10, 20, 30, 40);
  EXPECT_EQ(rgb_buf, (std::array<uint8_t, 6>{0, 0, 0, 20, 10, 30}));
  EXPECT_EQ(view.get_white_raw(), 0);
}

TEST(LedStripHelpers, ViewFallsBackToScratchBeforeSetup) {
  ESPColorCorrection correction;
  std::array<uint8_t, 4> buf{};
  // Any index is safe without a buffer, as is a buffer without effect data
  GRBW.view(nullptr, nullptr, 1000, &correction) = Color(1, 2, 3, 4);
  GRBW.view(nullptr, nullptr, 1000, &correction).set_effect_data(5);
  GRBW.view(buf.data(), nullptr, 0, &correction) = Color(1, 2, 3, 4);
  EXPECT_EQ(buf, (std::array<uint8_t, 4>{}));

  HelperStrip strip;
  strip.all() = Color(9, 9, 9);
  EXPECT_EQ(strip[0].get_red_raw(), 9);
}

TEST(LedStripHelpers, RefreshTooSoonHoldsTheFrame) {
  HelperStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  EXPECT_FALSE(strip.refresh_too_soon_(1000, 0, 0));
  EXPECT_TRUE(strip.refresh_too_soon_(1000, 500, 600));
  EXPECT_FALSE(strip.refresh_too_soon_(1100, 500, 600));
  // micros() wraps
  EXPECT_TRUE(strip.refresh_too_soon_(100, UINT32_MAX - 99, 300));
  EXPECT_FALSE(strip.refresh_too_soon_(200, UINT32_MAX - 99, 300));
}

TEST(LedStripHelpers, ZeroEffectDataSkipsAnUnallocatedBuffer) {
  std::array<uint8_t, 3> effect{1, 2, 3};
  HelperStrip::zero_effect_data_(effect.data(), effect.size());
  EXPECT_EQ(effect, (std::array<uint8_t, 3>{}));
  HelperStrip::zero_effect_data_(nullptr, 3);
}

}  // namespace esphome::light::testing
