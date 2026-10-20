#include <gtest/gtest.h>

#include "esphome/components/light/light_effect.h"
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

class NoopEffect : public LightEffect {
 public:
  using LightEffect::LightEffect;
  void apply() override {}
};

class ReachedListener : public LightTargetStateReachedListener {
 public:
  void on_light_target_state_reached() override { this->reached++; }
  int reached{0};
};

class PublishListener : public LightRemoteValuesListener {
 public:
  void on_light_remote_values_update() override { this->published++; }
  int published{0};
};

// Counts the target-reached calls that find the light off, as the on_turn_off trigger does
class OffReachedListener : public LightTargetStateReachedListener {
 public:
  explicit OffReachedListener(LightState *state) : state_(state) {}
  void on_light_target_state_reached() override { this->reached_off += !this->state_->current_values.is_on(); }
  int reached_off{0};

 protected:
  LightState *state_;
};

// An automation that turns the light off as soon as it is reported on
class TurnOffOnPublish : public LightRemoteValuesListener {
 public:
  explicit TurnOffOnPublish(LightState *state) : state_(state) {}
  void on_light_remote_values_update() override {
    if (this->state_->remote_values.is_on()) {
      this->state_->make_call().set_state(false).perform();
    }
  }

 protected:
  LightState *state_;
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

TEST(LightStateExternal, StopsARunningEffect) {
  CountingOutput output;
  LightState state(&output);
  NoopEffect effect("Noop");
  LightEffect *effects[] = {&effect};
  state.add_effects(effects, 1);
  state.loop();

  state.make_call().set_state(true).set_effect("Noop").perform();
  state.loop();
  ASSERT_EQ(state.get_current_effect_index(), 1u);
  const auto writes = output.writes;
  EXPECT_TRUE(state.set_external_state(on_at(0.5f)));
  EXPECT_EQ(state.get_current_effect_index(), 0u);
  EXPECT_EQ(state.remote_values, on_at(0.5f));
  state.loop();
  EXPECT_EQ(output.writes, writes);  // the stopped effect schedules no write either
}

TEST(LightStateExternal, PublishesWhenOnlyAnEffectStops) {
  CountingOutput output;
  LightState state(&output);
  NoopEffect effect("Noop");
  LightEffect *effects[] = {&effect};
  state.add_effects(effects, 1);
  PublishListener listener;
  state.add_remote_values_listener(&listener);
  state.loop();

  state.make_call().set_state(true).set_effect("Noop").perform();
  state.loop();
  const int published = listener.published;
  // The values stay, but the effect name clients show has to go
  EXPECT_TRUE(state.set_external_state(state.remote_values));
  EXPECT_EQ(state.get_current_effect_index(), 0u);
  EXPECT_EQ(listener.published, published + 1);
}

TEST(LightStateExternal, AnAutomationRunByThePublishReportsItsOwnCall) {
  CountingOutput output;
  LightState state(&output);
  OffReachedListener reached(&state);
  state.add_target_state_reached_listener(&reached);
  TurnOffOnPublish automation(&state);
  state.add_remote_values_listener(&automation);
  state.loop();

  EXPECT_TRUE(state.set_external_state(on_at(0.5f)));
  EXPECT_FALSE(state.remote_values.is_on());
  EXPECT_EQ(reached.reached_off, 1);  // once from the turn-off call, not again from the report
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
