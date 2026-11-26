#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <string>

#include "esphome/components/light/automation.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/hal.h"

// move_brightness(), which light.move calls, and stop_move(), which light.stop
// calls. The trigger tests record N for on_turn_on, F for on_turn_off and S for on_state, as test_light_triggers.cpp
// does.

namespace esphome::light::testing {

namespace {

class BrightnessOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::BRIGHTNESS});
    return traits;
  }
  void write_state(LightState *state) override {}
};

class OnOffOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::ON_OFF});
    return traits;
  }
  void write_state(LightState *state) override {}
};

class RgbOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::RGB});
    return traits;
  }
  void write_state(LightState *state) override {}
};

// Records the cold and warm white levels of each write
class ColdWarmWhiteOutput : public LightOutput {
 public:
  explicit ColdWarmWhiteOutput(ColorMode mode, float min_mireds = 153.0f, float max_mireds = 500.0f,
                               bool constant_brightness = false)
      : mode_(mode), min_mireds_(min_mireds), max_mireds_(max_mireds), constant_brightness_(constant_brightness) {}
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({this->mode_});
    traits.set_min_mireds(this->min_mireds_);
    traits.set_max_mireds(this->max_mireds_);
    return traits;
  }
  void write_state(LightState *state) override {
    state->current_values_as_cwww(&this->cold, &this->warm, this->constant_brightness_);
  }
  float cold{NAN};
  float warm{NAN};

 protected:
  ColorMode mode_;
  float min_mireds_;
  float max_mireds_;
  bool constant_brightness_;
};

// An interlocked RGBWW light: color or white, not both
class InterlockedRgbwwOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::RGB, ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(153.0f);
    traits.set_max_mireds(500.0f);
    return traits;
  }
  void write_state(LightState *state) override {}
};

class PublishListener : public LightRemoteValuesListener {
 public:
  void on_light_remote_values_update() override { this->published++; }
  int published{0};
};

// Attaches an on_turn_on, an on_turn_off and an on_state callback that each add their letter to `events`, built like
// the functions codegen generates for each light
class Recorder {
 public:
  explicit Recorder(LightState *state) {
    state->add_remote_values_callback([this, state]() {
      if (update_turned_on(this->last_on_, state->remote_values.is_on()))
        this->events += 'N';
      this->events += 'S';
    });
    state->add_target_state_reached_callback([this, state]() {
      if (!state->current_values.is_on())
        this->events += 'F';
    });
  }
  std::string take() {
    std::string events = std::move(this->events);
    this->events.clear();
    return events;
  }
  std::string events;

 protected:
  bool last_on_{false};
};

void turn_on_at(LightState &state, float brightness) {
  state.make_call().set_state(true).set_brightness(brightness).set_transition_length(0).perform();
  state.loop();
}

// Run loop() until the running transition ends
void finish(LightState &state) {
  for (int i = 0; i < 400 && state.is_transitioning(); i++) {
    delay(5);
    state.loop();
  }
  ASSERT_FALSE(state.is_transitioning());
}

// Let a transition of 1 s make some progress
void wait_a_little(LightState &state) {
  delay(40);
  state.loop();
}

// How fast the value changes per ms over a short while
float rate(LightState &state, const std::function<float()> &value) {
  const float before = value();
  const uint32_t start = millis();
  delay(40);
  state.loop();
  const float after = value();
  return (after - before) / static_cast<float>(millis() - start);
}

float brightness(LightState &state) { return state.current_values.get_brightness(); }

// A rate of 1 / full_range_ms per ms, give or take the timing
void expect_full_range_in(float rate, float full_range_ms) {
  EXPECT_GT(std::fabs(rate), 0.7f / full_range_ms);
  EXPECT_LT(std::fabs(rate), 1.4f / full_range_ms);
}

}  // namespace

// ---- Brightness ----

TEST(LightMoveActions, UpFromOffTurnsOnAtMinimumAndPublishesOnce) {
  BrightnessOutput output;
  LightState state(&output);
  state.setup();
  state.loop();
  PublishListener listener;
  state.add_remote_values_listener(&listener);

  move_brightness(&state, true, 1000, 0.1f, 0.8f);
  EXPECT_EQ(listener.published, 1);
  EXPECT_TRUE(state.remote_values.is_on());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 0.8f);
  EXPECT_TRUE(state.is_transitioning());
  EXPECT_FLOAT_EQ(brightness(state), 0.1f);
  expect_full_range_in(rate(state, [&]() { return brightness(state); }), 1000.0f);
  finish(state);
  EXPECT_FLOAT_EQ(brightness(state), 0.8f);
}

TEST(LightMoveActions, DownFromOffDoesNothing) {
  BrightnessOutput output;
  LightState state(&output);
  state.setup();
  state.loop();
  state.clear_called();

  move_brightness(&state, false, 1000, 0.1f, 1.0f);
  EXPECT_FALSE(state.called_since_clear());
  EXPECT_FALSE(state.remote_values.is_on());
}

TEST(LightMoveActions, UpWhileFadingOutContinuesFromTheCurrentBrightness) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.8f);
  state.make_call().set_state(false).set_transition_length(1000).perform();
  wait_a_little(state);
  const float fading = brightness(state);
  ASSERT_GT(fading, 0.1f);
  ASSERT_LT(fading, 0.8f);

  move_brightness(&state, true, 1000, 0.01f, 1.0f);
  EXPECT_TRUE(state.remote_values.is_on());
  EXPECT_FLOAT_EQ(brightness(state), fading);
  // 0.1 ms ago it was fading by 1/1000 per ms: now it rises
  EXPECT_GT(rate(state, [&]() { return brightness(state); }), 0.0f);
}

TEST(LightMoveActions, DownWhileFadingOutDoesNothing) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.8f);
  state.make_call().set_state(false).set_transition_length(1000).perform();
  state.clear_called();

  move_brightness(&state, false, 1000, 0.01f, 1.0f);
  EXPECT_FALSE(state.called_since_clear());
}

TEST(LightMoveActions, AtTheLimitDoesNothing) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.9f);
  state.clear_called();

  move_brightness(&state, true, 1000, 0.1f, 0.9f);
  move_brightness(&state, false, 1000, 0.9f, 1.0f);
  EXPECT_FALSE(state.called_since_clear());
}

TEST(LightMoveActions, OppositeMoveAtItsLimitStopsTheRunningMoveWithoutALoopTick) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 1.0f);

  move_brightness(&state, false, 1000, 0.01f, 1.0f);
  ASSERT_TRUE(state.is_transitioning());
  move_brightness(&state, true, 1000, 0.01f, 1.0f);  // at 100% already
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);
  delay(40);
  state.loop();
  EXPECT_FLOAT_EQ(brightness(state), 1.0f);

  // And the same at the minimum
  turn_on_at(state, 0.01f);
  move_brightness(&state, true, 1000, 0.01f, 1.0f);
  ASSERT_TRUE(state.is_transitioning());
  move_brightness(&state, false, 1000, 0.01f, 1.0f);
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 0.01f);
}

TEST(LightMoveActions, RepeatedMoveRestartsFromWhereTheLightIsAtTheNewSpeed) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.2f);

  move_brightness(&state, true, 4000, 0.01f, 1.0f);
  wait_a_little(state);
  const float reached = brightness(state);
  ASSERT_GT(reached, 0.2f);
  ASSERT_LT(reached, 0.5f);

  // Same direction, shorter: starts where the light is, not at the target the running move set
  move_brightness(&state, true, 400, 0.01f, 1.0f);
  EXPECT_FLOAT_EQ(brightness(state), reached);
  expect_full_range_in(rate(state, [&]() { return brightness(state); }), 400.0f);

  // The other way, at another speed
  const float up = brightness(state);
  move_brightness(&state, false, 1000, 0.01f, 1.0f);
  EXPECT_FLOAT_EQ(brightness(state), up);
  const float down_rate = rate(state, [&]() { return brightness(state); });
  EXPECT_LT(down_rate, 0.0f);
  expect_full_range_in(down_rate, 1000.0f);
  finish(state);
  EXPECT_FLOAT_EQ(brightness(state), 0.01f);
}

TEST(LightMoveActions, ShortDistanceStillMakesATransition) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.999f);

  // 0.1 ms of a 100 ms range
  move_brightness(&state, true, 100, 0.01f, 1.0f);
  EXPECT_TRUE(state.is_transitioning());
  state.stop_move();
  EXPECT_FALSE(state.is_transitioning());
}

TEST(LightMoveActions, StopFreezesTheMove) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.2f);

  move_brightness(&state, true, 1000, 0.01f, 1.0f);
  wait_a_little(state);
  const float reached = brightness(state);
  state.stop_move();
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), reached);
  delay(40);
  state.loop();
  EXPECT_FLOAT_EQ(brightness(state), reached);
}

TEST(LightMoveActions, DurationOfZeroOrLessChangesAtOnce) {
  for (int32_t duration : {0, -1, INT32_MIN}) {
    SCOPED_TRACE(duration);
    BrightnessOutput output;
    LightState state(&output);
    turn_on_at(state, 0.2f);

    move_brightness(&state, true, duration, 0.01f, 0.9f);
    EXPECT_FALSE(state.is_transitioning());
    EXPECT_FLOAT_EQ(brightness(state), 0.9f);
  }
}

TEST(LightMoveActions, DurationOfZeroDownToZeroTurnsOffAtOnce) {
  BrightnessOutput output;
  LightState state(&output);
  Recorder recorder(&state);
  state.setup();
  turn_on_at(state, 0.5f);
  recorder.take();

  move_brightness(&state, false, 0, 0.0f, 1.0f);
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_FALSE(state.remote_values.is_on());
  EXPECT_FALSE(state.current_values.is_on());
  EXPECT_EQ(recorder.take(), "FS");
}

TEST(LightMoveActions, LongestDurationMovesSlowly) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.2f);

  move_brightness(&state, true, INT32_MAX, 0.01f, 1.0f);
  EXPECT_TRUE(state.is_transitioning());
  delay(40);
  state.loop();
  EXPECT_NEAR(brightness(state), 0.2f, 0.001f);
}

TEST(LightMoveActions, NeedsBrightnessControl) {
  OnOffOutput output;
  LightState state(&output);
  state.loop();
  state.clear_called();

  move_brightness(&state, true, 1000, 0.01f, 1.0f);
  EXPECT_FALSE(state.called_since_clear());
}

// The second half of an interlocked RGB to white transition fades the white in: a move that takes over starts at the
// brightness shown, and does not fade the state in on the way
TEST(LightMoveActions, BrightnessMoveDuringTheFadeInOfAnInterlockedTransitionHoldsTheShownBrightness) {
  InterlockedRgbwwOutput output;
  LightState state(&output);
  state.make_call().set_state(true).set_brightness(0.8f).set_rgb(1.0f, 0.0f, 0.0f).set_transition_length(0).perform();
  state.loop();
  state.make_call().set_color_temperature(300.0f).set_transition_length(1000).perform();
  for (int i = 0; i < 16; i++)
    delay(40);
  state.loop();
  const LightColorValues shown = state.get_shown_values();
  ASSERT_EQ(shown.get_color_mode(), ColorMode::COLD_WARM_WHITE);
  ASSERT_GT(shown.get_state(), 0.0f);
  ASSERT_LT(shown.get_state(), 1.0f);
  const float effective = shown.get_brightness() * shown.get_state();

  move_brightness(&state, false, 1000, 0.0f, 1.0f);
  ASSERT_TRUE(state.is_moving());
  EXPECT_FLOAT_EQ(state.current_values.get_state(), 1.0f);
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), effective);
  wait_a_little(state);
  EXPECT_FLOAT_EQ(state.current_values.get_state(), 1.0f);
  EXPECT_LT(state.current_values.get_brightness(), effective);

  state.stop_move();
  EXPECT_LT(state.remote_values.get_brightness(), effective);
}

// A change of white or of color brightness that runs when the move starts does not go on through the move's call,
// which inherits the target
TEST(LightMoveActions, BrightnessMoveDuringAWhiteChangeHoldsTheShownWhite) {
  ColdWarmWhiteOutput output(ColorMode::RGB_COLOR_TEMPERATURE);
  LightState state(&output);
  state.make_call()
      .set_state(true)
      .set_brightness(1.0f)
      .set_white(0.2f)
      .set_color_temperature(300.0f)
      .set_transition_length(0)
      .perform();
  state.loop();
  state.make_call().set_white(1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  wait_a_little(state);
  const float white = state.get_shown_values().get_white();
  ASSERT_GT(white, 0.2f);
  ASSERT_LT(white, 1.0f);

  move_brightness(&state, false, 1000, 0.0f, 1.0f);
  ASSERT_TRUE(state.is_moving());
  EXPECT_FLOAT_EQ(state.remote_values.get_white(), white);
  state.stop_move();
  EXPECT_FLOAT_EQ(state.remote_values.get_white(), white);
}

TEST(LightMoveActions, BrightnessMoveDuringAColorBrightnessChangeHoldsTheShownColorBrightness) {
  ColdWarmWhiteOutput output(ColorMode::RGB_COLOR_TEMPERATURE);
  LightState state(&output);
  state.make_call()
      .set_state(true)
      .set_brightness(1.0f)
      .set_color_brightness(0.2f)
      .set_rgb(1.0f, 0.0f, 0.0f)
      .set_transition_length(0)
      .perform();
  state.loop();
  state.make_call().set_color_brightness(1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  wait_a_little(state);
  const float color_brightness = state.get_shown_values().get_color_brightness();
  ASSERT_GT(color_brightness, 0.2f);
  ASSERT_LT(color_brightness, 1.0f);

  move_brightness(&state, false, 1000, 0.0f, 1.0f);
  ASSERT_TRUE(state.is_moving());
  EXPECT_FLOAT_EQ(state.remote_values.get_color_brightness(), color_brightness);
  state.stop_move();
  EXPECT_FLOAT_EQ(state.remote_values.get_color_brightness(), color_brightness);
}

// The fade-out half of an interlocked change between RGB and white: the move stays in the mode shown
TEST(LightMoveActions, BrightnessMoveDuringTheFadeOutOfAnInterlockedTransitionStaysInTheShownMode) {
  InterlockedRgbwwOutput output;
  LightState state(&output);
  state.make_call().set_state(true).set_brightness(0.8f).set_rgb(1.0f, 0.0f, 0.0f).set_transition_length(0).perform();
  state.loop();
  state.make_call().set_color_temperature(300.0f).set_transition_length(1000).perform();
  for (int i = 0; i < 6; i++)
    delay(40);
  state.loop();
  const LightColorValues shown = state.get_shown_values();
  ASSERT_EQ(shown.get_color_mode(), ColorMode::RGB);
  ASSERT_EQ(state.remote_values.get_color_mode(), ColorMode::COLD_WARM_WHITE);
  ASSERT_LT(shown.get_state(), 1.0f);
  const float effective = shown.get_brightness() * shown.get_state();

  move_brightness(&state, false, 1000, 0.0f, 1.0f);
  ASSERT_TRUE(state.is_moving());
  EXPECT_EQ(state.remote_values.get_color_mode(), ColorMode::RGB);
  EXPECT_FLOAT_EQ(state.current_values.get_state(), 1.0f);
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), effective);
  state.stop_move();
  EXPECT_EQ(state.remote_values.get_color_mode(), ColorMode::RGB);
}

// ---- Color temperature ----

// The move of 1 ms is over before loop() applies its end; a move the other way at once must not go on to the end
TEST(LightMoveActions, MoveBackAfterTheTimeIsUpBeforeTheNextLoopDoesNotDim) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 1.0f);

  move_brightness(&state, false, 1, 0.01f, 1.0f);
  ASSERT_TRUE(state.is_transitioning());
  delay(3);
  move_brightness(&state, true, 1000, 0.01f, 1.0f);  // up, and the light is shown at 100%
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);
  state.loop();
  EXPECT_FLOAT_EQ(brightness(state), 1.0f);
  EXPECT_TRUE(state.current_values.is_on());
}

// A brightness move that takes over a normal transition to a warm target holds the channels where they are shown: the
// transition does not go on under the move, and the stop does not jump to its target. The light is at about 50% / 50%
// halfway.
TEST(LightMoveActions, BrightnessMoveOverAColorTemperatureTransitionDoesNotJumpAtTheEnd) {
  ColdWarmWhiteOutput output(ColorMode::COLD_WARM_WHITE);
  LightState state(&output);
  state.make_call()
      .set_state(true)
      .set_brightness(0.5f)
      .set_color_temperature(153.0f)
      .set_transition_length(0)
      .perform();
  state.loop();
  state.make_call().set_color_temperature(500.0f).set_transition_length(1000).perform();
  for (int i = 0; i < 8; i++)
    delay(40);
  state.loop();
  ASSERT_TRUE(state.is_transitioning());
  const LightColorValues shown = state.get_shown_values();
  ASSERT_GT(shown.get_warm_white(), 0.0f);
  ASSERT_GT(shown.get_cold_white(), 0.0f);

  move_brightness(&state, true, 2000, 0.01f, 1.0f);
  ASSERT_TRUE(state.is_moving());
  // Held where shown: remote_values is no longer the target of the transition
  EXPECT_FLOAT_EQ(state.remote_values.get_cold_white(), shown.get_cold_white());
  EXPECT_FLOAT_EQ(state.remote_values.get_warm_white(), shown.get_warm_white());
  for (int i = 0; i < 5; i++)
    wait_a_little(state);
  ASSERT_TRUE(state.is_transitioning());
  // The channels stay, and only the brightness changes the output
  EXPECT_NEAR(output.warm / output.cold, shown.get_warm_white() / shown.get_cold_white(), 0.01f);
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), shown.get_cold_white());
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), shown.get_warm_white());

  state.stop_move();
  state.loop();
  EXPECT_FLOAT_EQ(state.current_values.get_cold_white(), shown.get_cold_white());
  EXPECT_FLOAT_EQ(state.current_values.get_warm_white(), shown.get_warm_white());
  EXPECT_FLOAT_EQ(state.remote_values.get_cold_white(), shown.get_cold_white());
  EXPECT_FLOAT_EQ(state.remote_values.get_warm_white(), shown.get_warm_white());
}

// ---- Triggers ----

TEST(LightMoveActions, TriggersFromOffAreOnTurnOnAndOnStateOnce) {
  BrightnessOutput output;
  LightState state(&output);
  Recorder recorder(&state);
  state.setup();
  state.loop();
  recorder.take();

  move_brightness(&state, true, 50, 0.1f, 1.0f);
  EXPECT_EQ(recorder.take(), "NS");
  finish(state);
  EXPECT_EQ(recorder.take(), "");
}

TEST(LightMoveActions, TriggersOfAStopAreOnState) {
  BrightnessOutput output;
  LightState state(&output);
  Recorder recorder(&state);
  state.setup();
  state.loop();
  move_brightness(&state, true, 1000, 0.1f, 1.0f);
  recorder.take();
  delay(40);
  state.loop();

  state.stop_move();
  EXPECT_EQ(recorder.take(), "S");
}

TEST(LightMoveActions, TriggersOfAMoveDownToZeroTurnOffAtTheEnd) {
  BrightnessOutput output;
  LightState state(&output);
  Recorder recorder(&state);
  state.setup();
  turn_on_at(state, 0.5f);
  recorder.take();

  move_brightness(&state, false, 100, 0.0f, 1.0f);
  EXPECT_TRUE(state.remote_values.is_on());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 0.0f);
  EXPECT_EQ(recorder.take(), "S");
  finish(state);
  EXPECT_FALSE(state.remote_values.is_on());
  EXPECT_EQ(recorder.take(), "FS");
}

TEST(LightMoveActions, TriggersOfAStopAtZeroAfterTheTimeIsUpTurnOff) {
  BrightnessOutput output;
  LightState state(&output);
  Recorder recorder(&state);
  state.setup();
  turn_on_at(state, 0.0f);
  recorder.take();

  move_brightness(&state, true, 1, 0.0f, 1.0f);
  ASSERT_TRUE(state.is_transitioning());
  recorder.take();
  delay(3);
  state.stop_move();  // before the loop() that would apply the end, from 0% where the light is shown
  EXPECT_FALSE(state.remote_values.is_on());
  EXPECT_EQ(recorder.take(), "FS");
  state.loop();
  EXPECT_FALSE(state.current_values.is_on());
  EXPECT_EQ(recorder.take(), "");
}

TEST(LightMoveActions, TriggersOfAStopBeforeZeroLeaveTheLightOn) {
  BrightnessOutput output;
  LightState state(&output);
  Recorder recorder(&state);
  state.setup();
  turn_on_at(state, 0.5f);
  move_brightness(&state, false, 1000, 0.0f, 1.0f);
  recorder.take();
  delay(40);
  state.loop();

  state.stop_move();
  EXPECT_TRUE(state.remote_values.is_on());
  EXPECT_EQ(recorder.take(), "S");
  state.loop();
  EXPECT_EQ(recorder.take(), "");
}

TEST(LightMoveActions, UpFromOffWithAMinimumOfZeroTurnsOn) {
  BrightnessOutput output;
  LightState state(&output);
  Recorder recorder(&state);
  state.setup();
  state.loop();
  recorder.take();

  move_brightness(&state, true, 1000, 0.0f, 1.0f);
  EXPECT_TRUE(state.remote_values.is_on());
  EXPECT_EQ(recorder.take(), "NS");
  EXPECT_FLOAT_EQ(brightness(state), 0.0f);
  expect_full_range_in(rate(state, [&]() { return brightness(state); }), 1000.0f);
}

// ---- Saving ----

namespace {

struct Restored {
  bool restored{false};
  bool on{false};
  float brightness{-1.0f};
};
Restored &restored_values() {
  static Restored values;
  return values;
}

void record_restored(LightStateRTCState &state, bool restored) {
  restored_values() = {restored, state.state, state.brightness};
}

// What a light with the same preference key restores
Restored restore() {
  restored_values() = {};
  BrightnessOutput output;
  LightState state(&output);
  state.set_save_enabled(true);
  state.set_state_callback(record_restored);
  state.setup();
  return restored_values();
}

}  // namespace

TEST(LightMoveActions, MoveToZeroIsNotSavedAtTheStart) {
  BrightnessOutput output;
  LightState state(&output);
  state.set_save_enabled(true);
  state.setup();
  turn_on_at(state, 0.5f);

  move_brightness(&state, false, 1000, 0.0f, 1.0f);
  ASSERT_TRUE(state.remote_values.is_on());
  ASSERT_FLOAT_EQ(state.remote_values.get_brightness(), 0.0f);
  // A reset now restores the light as it was, not on at 0%, which restores as on at 100%
  const Restored restored = restore();
  EXPECT_TRUE(restored.restored);
  EXPECT_TRUE(restored.on);
  EXPECT_FLOAT_EQ(restored.brightness, 0.5f);
}

TEST(LightMoveActions, MoveToZeroIsSavedWhenItEnds) {
  BrightnessOutput output;
  LightState state(&output);
  state.set_save_enabled(true);
  state.setup();
  turn_on_at(state, 0.5f);

  move_brightness(&state, false, 100, 0.0f, 1.0f);
  finish(state);
  const Restored restored = restore();
  EXPECT_TRUE(restored.restored);
  EXPECT_FALSE(restored.on);
}

TEST(LightMoveActions, StopOfAMoveToZeroSavesWhereTheLightIs) {
  BrightnessOutput output;
  LightState state(&output);
  state.set_save_enabled(true);
  state.setup();
  turn_on_at(state, 0.5f);

  move_brightness(&state, false, 1000, 0.0f, 1.0f);
  wait_a_little(state);
  state.stop_move();
  const Restored restored = restore();
  EXPECT_TRUE(restored.on);
  EXPECT_FLOAT_EQ(restored.brightness, brightness(state));
  EXPECT_LT(restored.brightness, 0.5f);
}

// The light is off at 0%, not at the 100% the move was heading for, and that is what is saved
TEST(LightMoveActions, StopAtZeroAfterTheTimeIsUpPublishesAndSavesOffAtZero) {
  BrightnessOutput output;
  LightState state(&output);
  state.set_save_enabled(true);
  state.setup();
  turn_on_at(state, 0.0f);

  move_brightness(&state, true, 1, 0.0f, 1.0f);
  ASSERT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);
  delay(3);
  state.stop_move();
  EXPECT_FALSE(state.remote_values.is_on());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 0.0f);
  const Restored restored = restore();
  EXPECT_FALSE(restored.on);
  EXPECT_FLOAT_EQ(restored.brightness, 0.0f);

  // A plain turn-on from brightness 0 goes to 100%: LightCall makes the light visible
  state.make_call().set_state(true).set_transition_length(0).perform();
  state.loop();
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);
}

TEST(LightMoveActions, MoveToAnotherLimitIsSavedAtTheStart) {
  BrightnessOutput output;
  LightState state(&output);
  state.set_save_enabled(true);
  state.setup();
  turn_on_at(state, 0.5f);

  move_brightness(&state, true, 1000, 0.0f, 0.9f);
  const Restored restored = restore();
  EXPECT_FLOAT_EQ(restored.brightness, 0.9f);
}

}  // namespace esphome::light::testing
