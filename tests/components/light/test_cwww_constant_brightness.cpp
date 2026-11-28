#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

#include "esphome/components/light/light_call.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

namespace esphome::light::testing {

namespace {

constexpr float COLD_MIREDS = 153.0f;
constexpr float WARM_MIREDS = 500.0f;
constexpr ConstantBrightness MODE_DISABLED = ConstantBrightness::CONSTANT_BRIGHTNESS_DISABLED;
constexpr ConstantBrightness MODE_ENABLED = ConstantBrightness::CONSTANT_BRIGHTNESS_ENABLED;
constexpr ConstantBrightness MODE_PRIORITY = ConstantBrightness::CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY;

// A cold/warm white light, like the `cwww` platform.
class CwwwOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(COLD_MIREDS);
    traits.set_max_mireds(WARM_MIREDS);
    return traits;
  }
  void write_state(LightState *state) override {}
};

// validate_() runs transform_parameters_(), which turns the color temperature into cold/warm white.
class TestableLightCall : public LightCall {
 public:
  using LightCall::LightCall;
  using LightCall::validate_;
};

// Mireds at which the cold channel's linear share is cold_fraction.
float mireds_for(float cold_fraction) { return WARM_MIREDS - cold_fraction * (WARM_MIREDS - COLD_MIREDS); }

class CwwwLight {
 public:
  // gamma 0 leaves the light without a gamma table, so all values stay linear.
  explicit CwwwLight(double gamma) {
    if (gamma <= 0)
      return;
    for (int i = 1; i < 256; i++) {
      double raw = std::round(std::pow(i / 255.0, gamma) * 65535.0);
      this->table_.lut[i] = static_cast<uint16_t>(std::max(1.0, std::min(65535.0, raw)));
    }
    this->table_.gamma_x100 = static_cast<uint16_t>(gamma * 100);
    this->state.set_gamma_table(&this->table_);
  }

  LightColorValues values(bool on, float brightness, float mireds) {
    TestableLightCall call(&this->state);
    call.set_state(on).set_brightness(brightness).set_color_temperature(mireds);
    return call.validate_();
  }
  void set(bool on, float brightness, float mireds) {
    this->state.current_values = this->values(on, brightness, mireds);
  }

  std::pair<float, float> cwww(ConstantBrightness mode) {
    float cold, warm;
    this->state.current_values_as_cwww(&cold, &warm, mode);
    return {cold, warm};
  }
  std::pair<float, float> cwww(ConstantBrightness mode, float cold_relative, float warm_relative) {
    float cold, warm;
    this->state.current_values_as_cwww(&cold, &warm, mode, cold_relative, warm_relative);
    return {cold, warm};
  }
  std::pair<float, float> cwww(bool constant_brightness) {
    float cold, warm;
    this->state.current_values_as_cwww(&cold, &warm, constant_brightness);
    return {cold, warm};
  }

  CwwwOutput output;
  LightState state{&output};

 private:
  GammaTable table_{};
};

}  // namespace

// The bool overloads are the old API: false and true must map onto the disabled and enabled modes,
// and the enabled mode must keep producing the max/sum formula it always had.
TEST(CwwwConstantBrightness, BoolOverloadsMatchModes) {
  CwwwLight light(2.8);
  for (float brightness : {0.1f, 0.5f, 1.0f}) {
    for (float mireds : {153.0f, 250.0f, 326.5f, 400.0f, 500.0f}) {
      light.set(true, brightness, mireds);
      EXPECT_EQ(light.cwww(false), light.cwww(MODE_DISABLED));
      EXPECT_EQ(light.cwww(true), light.cwww(MODE_ENABLED));

      const auto &v = light.state.current_values;
      const float cw = light.state.gamma_correct_lut(v.get_cold_white());
      const float ww = light.state.gamma_correct_lut(v.get_warm_white());
      const float white = light.state.gamma_correct_lut(v.get_brightness());
      const float sum = cw + ww;
      auto [cold, warm] = light.cwww(MODE_ENABLED);
      EXPECT_EQ(cold, white * std::max(cw, ww) * cw / sum);
      EXPECT_EQ(warm, white * std::max(cw, ww) * ww / sum);
    }
  }
}

TEST(CwwwConstantBrightness, RgbwwForwardsMode) {
  CwwwLight light(2.8);
  light.set(true, 1.0f, 240.0f);
  for (auto mode : {MODE_DISABLED, MODE_ENABLED, MODE_PRIORITY}) {
    float red, green, blue, cold, warm;
    light.state.current_values_as_rgbww(&red, &green, &blue, &cold, &warm, mode);
    EXPECT_EQ(std::make_pair(cold, warm), light.cwww(mode));
    light.state.current_values_as_rgbww(&red, &green, &blue, &cold, &warm, mode, 0.62f, 1.0f);
    EXPECT_EQ(std::make_pair(cold, warm), light.cwww(mode, 0.62f, 1.0f));
  }
}

// Up to half brightness priority mode gives the same total output at every color temperature,
// twice what the enabled mode gives.
TEST(CwwwConstantBrightness, PriorityIsConstantUpToHalfBrightness) {
  CwwwLight light(0);
  for (float cold_fraction : {0.0f, 0.2f, 0.5f, 0.75f, 1.0f}) {
    light.set(true, 0.5f, mireds_for(cold_fraction));
    auto [cold, warm] = light.cwww(MODE_PRIORITY);
    EXPECT_NEAR(cold + warm, 1.0f, 1e-5f) << cold_fraction;
    EXPECT_NEAR(cold, cold_fraction, 1e-5f);
    auto [enabled_cold, enabled_warm] = light.cwww(MODE_ENABLED);
    EXPECT_NEAR(enabled_cold + enabled_warm, 0.5f, 1e-5f);
  }
}

// At full brightness the color temperature wins: the stronger channel is full and the ratio is kept.
TEST(CwwwConstantBrightness, PriorityFullBrightnessKeepsRatio) {
  struct Case {
    float cold_fraction, cold, warm;
  };
  for (const auto &c : {Case{0.5f, 1.0f, 1.0f}, Case{1.0f, 1.0f, 0.0f}, Case{0.0f, 0.0f, 1.0f},
                        Case{0.75f, 1.0f, 1.0f / 3.0f}, Case{0.25f, 1.0f / 3.0f, 1.0f}}) {
    CwwwLight light(0);
    light.set(true, 1.0f, mireds_for(c.cold_fraction));
    auto [cold, warm] = light.cwww(MODE_PRIORITY);
    EXPECT_NEAR(cold, c.cold, 1e-5f) << c.cold_fraction;
    EXPECT_NEAR(warm, c.warm, 1e-5f) << c.cold_fraction;
  }
}

// With gamma the balance works on linear output: half of the linear brightness
// (0.5^(1/2.8) on the slider) sums to full output, split by the linear color temperature fractions.
TEST(CwwwConstantBrightness, PriorityBalancesLinearOutputWithGamma) {
  CwwwLight light(2.8);
  for (float cold_fraction : {0.0f, 0.2f, 0.5f, 0.75f, 1.0f}) {
    light.set(true, std::pow(0.5f, 1.0f / 2.8f), mireds_for(cold_fraction));
    auto [cold, warm] = light.cwww(MODE_PRIORITY);
    EXPECT_NEAR(cold + warm, 1.0f, 1e-2f) << cold_fraction;
    EXPECT_NEAR(cold / (cold + warm), cold_fraction, 1e-2f) << cold_fraction;
  }
}

TEST(CwwwConstantBrightness, OffOrDarkGivesZero) {
  CwwwLight light(2.8);
  for (auto [on, brightness] : {std::pair{false, 1.0f}, std::pair{true, 0.0f}}) {
    light.set(on, brightness, 326.5f);
    for (auto mode : {MODE_ENABLED, MODE_PRIORITY})
      EXPECT_EQ(light.cwww(mode), std::make_pair(0.0f, 0.0f));
  }
}

// An interlocked RGBWW light in RGB mode has its white channels off.
TEST(CwwwConstantBrightness, NoWhiteChannelsInRgbMode) {
  CwwwLight light(2.8);
  LightColorValues values;
  values.set_color_mode(ColorMode::RGB);
  values.set_state(true);
  values.set_brightness(1.0f);
  light.state.current_values = values;
  EXPECT_EQ(light.cwww(MODE_PRIORITY), std::make_pair(0.0f, 0.0f));
}

// Midway through a cold to warm transition both stored channels are 0.5 before gamma. Priority
// drives each at gamma(0.5), twice what the enabled mode gives. Both are well below the settled
// output: the transition interpolates the stored channels, so constant output only holds once the
// color temperature has settled.
TEST(CwwwConstantBrightness, PriorityTransitionMidpoint) {
  CwwwLight light(2.8);
  auto start = light.values(true, 1.0f, COLD_MIREDS);
  auto end = light.values(true, 1.0f, WARM_MIREDS);
  light.state.current_values = LightColorValues::lerp(start, end, 0.5f);
  auto [cold, warm] = light.cwww(MODE_PRIORITY);
  EXPECT_NEAR(cold, std::pow(0.5f, 2.8f), 1e-3f);
  EXPECT_FLOAT_EQ(cold, warm);
  EXPECT_FLOAT_EQ(cold, 2.0f * light.cwww(MODE_ENABLED).first);
}

// Equal relative brightnesses are the overload without them, in every mode.
TEST(CwwwConstantBrightness, EqualRelativeBrightnessChangesNothing) {
  CwwwLight light(2.8);
  for (float brightness : {0.1f, 0.9f, 1.0f}) {
    for (float mireds : {153.0f, 240.0f, 326.5f, 500.0f}) {
      light.set(true, brightness, mireds);
      for (auto mode : {MODE_DISABLED, MODE_ENABLED, MODE_PRIORITY})
        EXPECT_EQ(light.cwww(mode, 1.0f, 1.0f), light.cwww(mode));
    }
  }
}

// Without constant brightness the light is split, not the duty: the weighted light of the two channels follows
// the color temperature, the stronger-needed channel is at the brightness, and both are full where the light balances.
TEST(CwwwConstantBrightness, RelativeBrightnessSplitsLightWhenDisabled) {
  constexpr float cold_relative = 0.71f;
  constexpr float warm_relative = 1.0f;
  for (double gamma : {0.0, 2.8}) {
    CwwwLight light(gamma);
    const float balanced = cold_relative / (cold_relative + warm_relative);

    // Both channels are full where their light balances, at full brightness.
    light.set(true, 1.0f, mireds_for(balanced));
    auto [cold, warm] = light.cwww(MODE_DISABLED, cold_relative, warm_relative);
    EXPECT_NEAR(cold, 1.0f, 1e-3f) << gamma;
    EXPECT_NEAR(warm, 1.0f, 1e-3f) << gamma;

    for (float cold_fraction : {0.0f, 0.2f, 0.5f, 0.75f, 1.0f}) {
      light.set(true, 1.0f, mireds_for(cold_fraction));
      std::tie(cold, warm) = light.cwww(MODE_DISABLED, cold_relative, warm_relative);
      // The color temperature is the real mix of the light
      EXPECT_NEAR(cold_relative * cold / (cold_relative * cold + warm_relative * warm), cold_fraction, 1e-3f)
          << gamma << " " << cold_fraction;
      // The stronger-needed channel is at full brightness, the other below
      EXPECT_NEAR(std::max(cold, warm), 1.0f, 1e-3f) << gamma << " " << cold_fraction;

      // Half brightness scales both channels, keeping the mix
      light.set(true, 0.5f, mireds_for(cold_fraction));
      auto [half_cold, half_warm] = light.cwww(MODE_DISABLED, cold_relative, warm_relative);
      EXPECT_NEAR(std::max(half_cold, half_warm), light.state.gamma_correct_lut(0.5f), 1e-3f)
          << gamma << " " << cold_fraction;
      if (cold + warm > 0.01f)
        EXPECT_NEAR(half_cold / (half_cold + half_warm), cold / (cold + warm), 1e-3f) << gamma << " " << cold_fraction;
    }
  }
}

// At a low brightness the quantised gamma table is not lut(a) * lut(b), so the brightness is gamma-corrected on its
// own: where the light balances, the channels still get equal duties at the extremes of the ratio (a table's step is a
// few percent here; scaling the brightness into the table instead boosted one channel about 100x).
TEST(CwwwConstantBrightness, RelativeBrightnessStaysBalancedAtLowBrightnessWhenDisabled) {
  CwwwLight light(2.8);
  for (auto [cold_relative, warm_relative] : {std::pair{0.01f, 1.0f}, std::pair{1.0f, 0.01f}}) {
    const float balanced = cold_relative / (cold_relative + warm_relative);
    for (float brightness : {0.02f, 0.05f, 0.1f, 0.5f}) {
      light.set(true, brightness, mireds_for(balanced));
      auto [cold, warm] = light.cwww(MODE_DISABLED, cold_relative, warm_relative);
      EXPECT_GT(cold, 0.0f) << cold_relative << " " << brightness;
      EXPECT_NEAR(cold / warm, 1.0f, 0.1f) << cold_relative << " " << brightness;
    }
  }
}

// A cold strip giving 0.71 of the warm strip's light: the modes balance the light, not the output level.
TEST(CwwwConstantBrightness, RelativeBrightnessBalancesLight) {
  constexpr float cold_relative = 0.71f;
  constexpr float warm_relative = 1.0f;
  CwwwLight light(0);

  // Priority at full brightness drives both channels fully where their light balances.
  light.set(true, 1.0f, mireds_for(cold_relative / (cold_relative + warm_relative)));
  auto [cold, warm] = light.cwww(MODE_PRIORITY, cold_relative, warm_relative);
  EXPECT_NEAR(cold, 1.0f, 1e-5f);
  EXPECT_NEAR(warm, 1.0f, 1e-5f);

  for (float cold_fraction : {0.0f, 0.2f, 0.5f, 0.75f, 1.0f}) {
    // Priority keeps the light constant while the cold strip alone can still give it.
    light.set(true, 0.4f, mireds_for(cold_fraction));
    std::tie(cold, warm) = light.cwww(MODE_PRIORITY, cold_relative, warm_relative);
    EXPECT_NEAR(cold_relative * cold + warm_relative * warm, 0.4f * (cold_relative + warm_relative), 1e-5f)
        << cold_fraction;

    // Enabled keeps the light at what the weaker strip gives alone, without driving a channel past full.
    light.set(true, 1.0f, mireds_for(cold_fraction));
    std::tie(cold, warm) = light.cwww(MODE_ENABLED, cold_relative, warm_relative);
    EXPECT_NEAR(cold_relative * cold + warm_relative * warm, cold_relative, 1e-5f) << cold_fraction;
    EXPECT_LE(cold, 1.0f);
    EXPECT_LE(warm, 1.0f);
  }
}

}  // namespace esphome::light::testing
