#include <gtest/gtest.h>

#include "esphome/components/light/color_mode.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/light/base_light_effects.h"

namespace esphome::light::testing {

namespace {

constexpr float COLD_MIRED = 1000000.0f / 4100.0f;
constexpr float WARM_MIRED = 1000000.0f / 4000.0f;

class StrobeOutput : public LightOutput {
 public:
  explicit StrobeOutput(ColorMode mode, float min_mireds = COLD_MIRED, float max_mireds = WARM_MIRED)
      : mode_(mode), min_mireds_(min_mireds), max_mireds_(max_mireds) {}
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({this->mode_});
    traits.set_min_mireds(this->min_mireds_);
    traits.set_max_mireds(this->max_mireds_);
    return traits;
  }
  void write_state(LightState *state) override {}

 protected:
  ColorMode mode_;
  float min_mireds_;
  float max_mireds_;
};

class TestableStrobe : public StrobeLightEffect {
 public:
  TestableStrobe() : StrobeLightEffect("Strobe") {}
  void advance() {
    this->last_switch_ = millis() - this->duration_(this->at_color_);
    this->apply();
  }
};

StrobeLightEffectColor step_at(float temperature, bool use_temperature) {
  return {
      .state = 1.0f,
      .brightness = 1.0f,
      .color_brightness = 1.0f,
      .red = 1.0f,
      .green = 1.0f,
      .blue = 1.0f,
      .white = 1.0f,
      .color_temperature = temperature,
      .cold_white = 1.0f,
      .warm_white = 1.0f,
      .duration = 500,
      .transition_length = 0,
      .color_mode = ColorMode::UNKNOWN,
      .use_color_temperature = use_temperature,
  };
}

void initialize_strobe(TestableStrobe &effect, LightState &state, ColorMode mode, const StrobeLightEffectColor *steps,
                       size_t count) {
  state.current_values.set_color_mode(mode);
  state.remote_values.set_color_mode(mode);
  effect.set_colors(steps, count);
  effect.init_internal(&state);
  effect.start();
}

class StrobeTemperature : public ::testing::TestWithParam<ColorMode> {};

}  // namespace

TEST_P(StrobeTemperature, AlternatesColdAndWarmChannels) {
  StrobeOutput output(GetParam());
  LightState state(&output);
  TestableStrobe effect;
  const StrobeLightEffectColor steps[] = {step_at(COLD_MIRED, true), step_at(WARM_MIRED, true)};
  initialize_strobe(effect, state, GetParam(), steps, 2);

  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 1.0f);
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 0.0f);

  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 0.0f);
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 1.0f);

  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 1.0f);
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 0.0f);
}

TEST_P(StrobeTemperature, KeepsDefaultChannelsWithoutAValidTemperatureRange) {
  constexpr float ranges[][2] = {
      {0.0f, 0.0f}, {0.0f, WARM_MIRED}, {COLD_MIRED, 0.0f}, {COLD_MIRED, COLD_MIRED}, {WARM_MIRED, COLD_MIRED}};
  for (const auto &range : ranges) {
    SCOPED_TRACE(::testing::Message() << "min_mireds=" << range[0] << ", max_mireds=" << range[1]);
    StrobeOutput output(GetParam(), range[0], range[1]);
    LightState state(&output);
    TestableStrobe effect;
    const StrobeLightEffectColor steps[] = {step_at(COLD_MIRED, true), step_at(WARM_MIRED, true)};
    initialize_strobe(effect, state, GetParam(), steps, 2);

    state.remote_values.set_cold_white(0.0f);
    state.remote_values.set_warm_white(0.0f);
    effect.advance();
    EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 1.0f);
    EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 1.0f);

    state.remote_values.set_cold_white(0.25f);
    state.remote_values.set_warm_white(0.5f);
    effect.advance();
    EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 1.0f);
    EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 1.0f);
  }
}

INSTANTIATE_TEST_SUITE_P(WhiteModes, StrobeTemperature,
                         ::testing::Values(ColorMode::COLD_WARM_WHITE, ColorMode::RGB_COLD_WARM_WHITE));

TEST(StrobeTemperature, ExplicitChannelsTakePriorityOverTemperature) {
  StrobeOutput output(ColorMode::COLD_WARM_WHITE);
  LightState state(&output);
  TestableStrobe effect;
  StrobeLightEffectColor steps[] = {step_at(COLD_MIRED, false), step_at(WARM_MIRED, false)};
  steps[0].cold_white = 0.25f;
  steps[0].warm_white = 0.5f;
  steps[1].cold_white = 0.0f;
  initialize_strobe(effect, state, ColorMode::COLD_WARM_WHITE, steps, 2);

  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 0.25f);
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 0.5f);

  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 0.0f);
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 1.0f);
}

TEST(StrobeTemperature, NativeColorTemperatureIsPreserved) {
  StrobeOutput output(ColorMode::COLOR_TEMPERATURE);
  LightState state(&output);
  TestableStrobe effect;
  const StrobeLightEffectColor steps[] = {step_at(COLD_MIRED, true), step_at(WARM_MIRED, true)};
  initialize_strobe(effect, state, ColorMode::COLOR_TEMPERATURE, steps, 2);

  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_color_temperature(), COLD_MIRED);
  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_color_temperature(), WARM_MIRED);
}

TEST(StrobeTemperature, DefaultWhiteChannelsRemainFull) {
  StrobeOutput output(ColorMode::COLD_WARM_WHITE);
  LightState state(&output);
  TestableStrobe effect;
  const StrobeLightEffectColor steps[] = {step_at(0.0f, false), step_at(0.0f, false)};
  initialize_strobe(effect, state, ColorMode::COLD_WARM_WHITE, steps, 2);

  effect.advance();
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), 1.0f);
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), 1.0f);
}

TEST(StrobeTemperature, DefaultStrobeStillSwitchesAnOnOffLight) {
  StrobeOutput output(ColorMode::ON_OFF);
  LightState state(&output);
  TestableStrobe effect;
  StrobeLightEffectColor steps[] = {step_at(0.0f, false), step_at(0.0f, false)};
  steps[1].state = 0.0f;
  initialize_strobe(effect, state, ColorMode::ON_OFF, steps, 2);

  effect.advance();
  EXPECT_TRUE(state.current_values.is_on());
  effect.advance();
  EXPECT_FALSE(state.current_values.is_on());
  effect.advance();
  EXPECT_TRUE(state.current_values.is_on());
}

}  // namespace esphome::light::testing
