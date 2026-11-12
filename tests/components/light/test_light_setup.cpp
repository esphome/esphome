#include <gtest/gtest.h>

#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

namespace esphome::light::testing {

namespace {

class CountingOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::BRIGHTNESS});
    return traits;
  }
  void write_state(LightState *state) override { this->writes++; }
  int writes{0};
};

// Turns the light on at start-up without a color mode, as restore_mode: ALWAYS_ON does
void start_on(LightStateRTCState &state, bool restored) { state.state = true; }

class CountingEffect : public LightEffect {
 public:
  using LightEffect::LightEffect;
  void apply() override { this->applies++; }
  int applies{0};
};

void start_on_with_effect(LightStateRTCState &state, bool restored) {
  state.state = true;
  state.effect = 1;
}

}  // namespace

TEST(LightStateSetup, StartsOnWithoutAFadeWhenNoColorModeIsKnown) {
  CountingOutput output;
  LightState state(&output);
  state.set_default_transition_length(1000);
  state.set_state_callback(start_on);
  state.setup();
  // A transition would leave the light off until loop() runs it
  EXPECT_TRUE(state.current_values.is_on());
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), 1.0f);
}

TEST(LightStateSetup, LeavesTheWriteToLoopByDefault) {
  CountingOutput output;
  LightState state(&output);
  state.set_state_callback(start_on);
  state.setup();
  EXPECT_EQ(output.writes, 0);
}

TEST(LightStateSetup, WritesDuringSetupWithWriteOnSetup) {
  CountingOutput output;
  LightState state(&output);
  state.set_default_transition_length(1000);
  state.set_state_callback(start_on);
  state.set_write_on_setup(true);
  state.setup();
  EXPECT_EQ(output.writes, 1);
  EXPECT_TRUE(state.current_values.is_on());
}

TEST(LightStateSetup, LeavesARestoredEffectToLoopWithWriteOnSetup) {
  CountingOutput output;
  LightState state(&output);
  CountingEffect effect("Counting");
  LightEffect *effects[] = {&effect};
  state.add_effects(effects, 1);
  state.set_state_callback(start_on_with_effect);
  state.set_write_on_setup(true);
  state.setup();
  EXPECT_EQ(state.get_effect_name(), "Counting");
  EXPECT_EQ(effect.applies, 0);
}

}  // namespace esphome::light::testing
