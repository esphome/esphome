#include <gtest/gtest.h>

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

}  // namespace esphome::cwww::testing
