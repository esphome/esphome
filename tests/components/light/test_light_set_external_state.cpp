#include <gtest/gtest.h>

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

namespace esphome::light::testing {

namespace {

// Counts the writes, like an output that mirrors a light elsewhere and sends each one on
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

class ReachedListener : public LightTargetStateReachedListener {
 public:
  void on_light_target_state_reached() override { this->reached++; }
  int reached{0};
};

LightColorValues on_at(float brightness) {
  LightColorValues values;
  values.set_color_mode(ColorMode::BRIGHTNESS);
  values.set_state(true);
  values.set_brightness(brightness);
  return values;
}

}  // namespace

TEST(LightStateExternal, TakesTheValuesWithoutAWrite) {
  CountingOutput output;
  LightState state(&output);
  ReachedListener listener;
  state.add_target_state_reached_listener(&listener);
  state.loop();  // the initial write of a fresh state
  const int writes = output.writes;

  EXPECT_TRUE(state.set_external_state(on_at(0.5f)));
  EXPECT_EQ(state.remote_values, on_at(0.5f));
  EXPECT_EQ(state.current_values, on_at(0.5f));
  EXPECT_EQ(listener.reached, 1);
  state.loop();
  EXPECT_EQ(output.writes, writes);
}

TEST(LightStateExternal, RefusesWhileALocalChangeWaitsToBeWritten) {
  CountingOutput output;
  LightState state(&output);
  state.loop();

  state.make_call().set_state(true).set_brightness(0.8f).perform();
  EXPECT_FALSE(state.set_external_state(on_at(0.5f)));
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 0.8f);
  state.loop();  // the local change is written
  EXPECT_TRUE(state.set_external_state(on_at(0.5f)));
}

TEST(LightStateExternal, RefusesDuringATransition) {
  CountingOutput output;
  LightState state(&output);
  state.loop();

  state.make_call().set_state(true).set_brightness(1.0f).set_transition_length(1000).perform();
  state.loop();
  EXPECT_TRUE(state.is_transitioning());
  EXPECT_FALSE(state.set_external_state(on_at(0.5f)));
}

TEST(LightStateExternal, SaysNothingWhenNothingChanges) {
  CountingOutput output;
  LightState state(&output);
  ReachedListener listener;
  state.add_target_state_reached_listener(&listener);
  state.loop();

  EXPECT_TRUE(state.set_external_state(on_at(0.5f)));
  EXPECT_TRUE(state.set_external_state(on_at(0.5f)));
  EXPECT_EQ(listener.reached, 1);
}

}  // namespace esphome::light::testing
