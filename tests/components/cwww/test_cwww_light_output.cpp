#include <gtest/gtest.h>

#include <algorithm>

#include "esphome/components/cwww/cwww_light_output.h"

namespace esphome::cwww::testing {

namespace {

class RecordingOutput : public output::FloatOutput {
 public:
  float level{-1.0f};

 protected:
  void write_state(float state) override { this->level = state; }
};

// A light at mid color temperature, where every mode gives different levels.
void set_values(light::LightState &state) {
  state.current_values.set_color_mode(light::ColorMode::COLD_WARM_WHITE);
  state.current_values.set_state(true);
  state.current_values.set_brightness(0.3f);
  state.current_values.set_cold_white(1.0f);
  state.current_values.set_warm_white(0.5f);
}

}  // namespace

// Lambdas written before color_temperature_priority still pass a bool.
TEST(CWWWLightOutput, BoolConstantBrightnessSetter) {
  RecordingOutput cold, warm;
  CWWWLightOutput output;
  output.set_cold_white(&cold);
  output.set_warm_white(&warm);
  light::LightState state(&output);
  set_values(state);

  for (bool constant_brightness : {false, true}) {
    output.set_constant_brightness(constant_brightness);
    output.write_state(&state);
    float expected_cold, expected_warm;
    state.current_values_as_cwww(&expected_cold, &expected_warm, constant_brightness);
    EXPECT_FLOAT_EQ(cold.level, expected_cold) << constant_brightness;
    EXPECT_FLOAT_EQ(warm.level, expected_warm) << constant_brightness;
  }
}

// write_state hands each channel its own level and relative brightness.
TEST(CWWWLightOutput, WritesRelativeBrightness) {
  RecordingOutput cold, warm;
  CWWWLightOutput output;
  output.set_cold_white(&cold);
  output.set_warm_white(&warm);
  output.set_constant_brightness(light::ConstantBrightness::CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY);
  output.set_white_relative_brightness(0.62f, 1.0f);
  light::LightState state(&output);
  set_values(state);
  output.write_state(&state);

  float expected_cold, expected_warm;
  state.current_values_as_cwww(&expected_cold, &expected_warm,
                               light::ConstantBrightness::CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY, 0.62f, 1.0f);
  EXPECT_FLOAT_EQ(cold.level, expected_cold);
  EXPECT_FLOAT_EQ(warm.level, expected_warm);
  EXPECT_NE(cold.level, warm.level);
}

// A lambda may switch the mode of a weighted light at runtime; each mode keeps its property.
TEST(CWWWLightOutput, RuntimeModeSwitchKeepsEachMode) {
  constexpr float cold_relative = 0.92f / 1.30f;  // as codegen normalises 0.92 : 1.30
  RecordingOutput cold, warm;
  CWWWLightOutput output;
  output.set_cold_white(&cold);
  output.set_warm_white(&warm);
  output.set_constant_brightness(light::ConstantBrightness::CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY);
  output.set_white_relative_brightness(cold_relative, 1.0f);
  light::LightState state(&output);
  state.current_values.set_color_mode(light::ColorMode::COLD_WARM_WHITE);
  state.current_values.set_state(true);
  state.current_values.set_brightness(1.0f);
  // Without a gamma table the stored channels are the linear color temperature fractions, the larger 1.0
  auto set_cold_fraction = [&](float cold_fraction) {
    const float larger = std::max(cold_fraction, 1.0f - cold_fraction);
    state.current_values.set_cold_white(cold_fraction / larger);
    state.current_values.set_warm_white((1.0f - cold_fraction) / larger);
  };

  // Switched to enabled, the light stays at what the weaker cold strip gives alone.
  output.set_constant_brightness(true);
  for (float cold_fraction : {0.0f, 0.3f, 0.5f, 0.8f, 1.0f}) {
    set_cold_fraction(cold_fraction);
    output.write_state(&state);
    EXPECT_NEAR(cold_relative * cold.level + warm.level, cold_relative, 1e-5f) << cold_fraction;
  }

  // Switched to disabled, the light splits by color temperature with the stronger-needed channel at full.
  output.set_constant_brightness(false);
  for (float cold_fraction : {0.0f, 0.3f, 0.5f, 0.8f, 1.0f}) {
    set_cold_fraction(cold_fraction);
    output.write_state(&state);
    EXPECT_NEAR(cold_relative * cold.level / (cold_relative * cold.level + warm.level), cold_fraction, 1e-5f)
        << cold_fraction;
    EXPECT_NEAR(std::max(cold.level, warm.level), 1.0f, 1e-5f) << cold_fraction;
  }

  // Back to priority, full brightness drives both channels fully where their light balances.
  output.set_constant_brightness(light::ConstantBrightness::CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY);
  set_cold_fraction(cold_relative / (cold_relative + 1.0f));
  output.write_state(&state);
  EXPECT_NEAR(cold.level, 1.0f, 1e-5f);
  EXPECT_NEAR(warm.level, 1.0f, 1e-5f);
}

}  // namespace esphome::cwww::testing
