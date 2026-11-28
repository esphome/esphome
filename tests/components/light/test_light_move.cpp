#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/base_light_effects.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/transformers.h"
#include "esphome/core/hal.h"

// LightState::perform_move() and stop_move()

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

// Makes a call of its own while a call is validated, as a log callback of a VERBOSE perform() would: before the call
// starts its transition
class CallingOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::BRIGHTNESS});
    if (this->state != nullptr && !this->calling) {
      this->calling = true;
      this->state->make_call().set_brightness(0.3f).set_transition_length(10000).perform();
      this->calling = false;
    }
    return traits;
  }
  void write_state(LightState *state) override {}
  LightState *state{nullptr};
  bool calling{false};
};

// Records the RGB levels of each write
class RgbOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::RGB});
    return traits;
  }
  void write_state(LightState *state) override { state->current_values_as_rgb(&this->red, &this->green, &this->blue); }
  float red{NAN};
  float green{NAN};
  float blue{NAN};
};

// An RGB strip without a buffer: only the values the light state keeps matter here
class NullStrip : public AddressableLight {
 public:
  int32_t size() const override { return 1; }
  void clear_effect_data() override {}
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::RGB});
    return traits;
  }
  void write_state(LightState *state) override {}
};

// A one-pixel RGB strip with a buffer, as the LED outputs have: what a transition draws can be read back
class BufferedStrip : public AddressableLight {
 public:
  BufferedStrip()
      : internal_accessor_(this->buffer_, this->buffer_ + 4, light::ChannelColors{0, 1, 2, 3}, &this->correction_) {
    this->accessor_ = &this->internal_accessor_;
  }
  int32_t size() const override { return 1; }
  void clear_effect_data() override {}
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::RGB});
    return traits;
  }
  void write_state(LightState *state) override {}
  float drawn() { return (*this)[0].get().red / 255.0f; }

 protected:
  uint8_t buffer_[5]{};
  light::CorrectedBufferColorAccessor internal_accessor_;
};

class PublishListener : public LightRemoteValuesListener {
 public:
  void on_light_remote_values_update() override { this->published++; }
  int published{0};
};

// Starts a transition to on at 0% from the first publish it sees
class TransitionOnPublish : public LightRemoteValuesListener {
 public:
  explicit TransitionOnPublish(LightState *state) : state_(state) {}
  void on_light_remote_values_update() override {
    if (this->armed_) {
      this->armed_ = false;
      this->state_->make_call().set_state(true).set_brightness(0.0f).set_transition_length(20).perform();
    }
  }

 protected:
  LightState *state_;
  bool armed_{true};
};

// Counts the notifications, and on the first one issues a command of its own
class ReachedListener : public LightTargetStateReachedListener {
 public:
  explicit ReachedListener(LightState *state = nullptr) : state_(state) {}
  void on_light_target_state_reached() override {
    if (this->reached++ == 0 && this->state_ != nullptr)
      this->state_->make_call().set_state(true).set_brightness(0.7f).set_transition_length(0).perform();
  }
  int reached{0};

 protected:
  LightState *state_;
};

// Lets a test move a transition forward without waiting
class TestTransition : public LightTransitionTransformer {
 public:
  using LightTransitionTransformer::LightTransitionTransformer;
  void advance(uint32_t ms) { this->start_time_ -= ms; }
};

LightColorValues on_at(float brightness) {
  LightColorValues values;
  values.set_color_mode(ColorMode::BRIGHTNESS);
  values.set_state(true);
  values.set_brightness(brightness);
  return values;
}

float brightness_after_quarter(bool linear) {
  TestTransition transition(linear);
  transition.setup(on_at(0.2f), on_at(0.6f), 1000);
  transition.advance(250);
  auto values = transition.apply();
  return values.has_value() ? values->get_brightness() : NAN;
}

// Turn the light on at `brightness` and write it, so no change is pending
void turn_on_at(LightState &state, float brightness) {
  state.make_call().set_state(true).set_brightness(brightness).set_transition_length(0).perform();
  state.loop();
}

void move_to(LightState &state, float target, uint32_t length) {
  auto call = state.make_call();
  call.set_state(true).set_brightness(target).set_transition_length(length);
  state.perform_move(call, state.get_shown_values());
}

// Run loop() until the running transition ends
void finish(LightState &state) {
  for (int i = 0; i < 200 && state.is_transitioning(); i++) {
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

}  // namespace

TEST(LightMove, LinearTransitionRunsAtConstantSpeed) {
  EXPECT_NEAR(brightness_after_quarter(true), 0.3f, 0.01f);
  // The default eases in: a quarter of the way in time is about a tenth of the way in value
  EXPECT_NEAR(brightness_after_quarter(false), 0.2f + 0.4f * 0.1035f, 0.01f);
}

TEST(LightMove, StopFreezesAMove) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.2f);

  move_to(state, 1.0f, 1000);
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);
  wait_a_little(state);
  const float reached = state.current_values.get_brightness();
  EXPECT_GT(reached, 0.2f);
  EXPECT_LT(reached, 1.0f);

  state.stop_move();
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_EQ(state.remote_values, state.current_values);
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), reached);
  wait_a_little(state);
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), reached);
}

// Only a move is frozen: a regular transition (a turn off too), a flash and an effect run on
TEST(LightMove, StopLeavesATransitionRunning) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.8f);

  state.make_call().set_brightness(1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  state.clear_called();
  state.stop_move();
  EXPECT_TRUE(state.is_transitioning());
  EXPECT_FALSE(state.called_since_clear());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);

  state.make_call().set_state(false).set_transition_length(1000).perform();
  wait_a_little(state);
  state.clear_called();
  state.stop_move();
  EXPECT_TRUE(state.is_transitioning());
  EXPECT_FALSE(state.called_since_clear());
  EXPECT_FALSE(state.remote_values.is_on());
}

TEST(LightMove, StopWithNothingChangingDoesNothing) {
  BrightnessOutput output;
  LightState state(&output);
  PublishListener listener;
  state.add_remote_values_listener(&listener);
  turn_on_at(state, 0.5f);
  state.clear_called();
  const int published = listener.published;

  state.stop_move();
  EXPECT_FALSE(state.called_since_clear());
  EXPECT_EQ(listener.published, published);
}

TEST(LightMove, StopLeavesAFlashRunning) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.2f);
  state.make_call().set_brightness(1.0f).set_flash_length(10000).perform();
  state.clear_called();

  state.stop_move();
  EXPECT_TRUE(state.is_transitioning());
  EXPECT_FALSE(state.called_since_clear());
}

TEST(LightMove, StopLeavesAPulseEffectRunning) {
  BrightnessOutput output;
  LightState state(&output);
  PulseLightEffect pulse("Pulse");
  pulse.set_transition_on_length(1000);
  pulse.set_transition_off_length(1000);
  pulse.set_update_interval(2000);
  LightEffect *effects[] = {&pulse};
  state.add_effects(effects, 1);
  state.setup();
  state.loop();
  state.make_call().set_state(true).set_effect("Pulse").perform();
  state.loop();
  wait_a_little(state);
  ASSERT_TRUE(state.is_transitioning());
  state.clear_called();

  state.stop_move();
  EXPECT_TRUE(state.is_transitioning());
  EXPECT_EQ(state.get_current_effect_index(), 1u);
  EXPECT_FALSE(state.called_since_clear());
}

TEST(LightMove, StopLeavesAnAddressableTransitionRunning) {
  NullStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  turn_on_at(state, 0.2f);

  // The strip's own transition writes the LEDs and leaves current_values where it started
  state.make_call().set_brightness(1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  wait_a_little(state);
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), 0.2f);
  const float shown = state.get_shown_values().get_brightness();
  EXPECT_GT(shown, 0.2f);
  EXPECT_LT(shown, 1.0f);
  state.clear_called();

  state.stop_move();
  EXPECT_TRUE(state.is_transitioning());
  EXPECT_FALSE(state.called_since_clear());
}

// Where the strip's own transition is shown is where its last frame got to, not where the clock says
TEST(LightMove, ShownValuesOfAnAddressableTransitionAreOfTheLastFrame) {
  NullStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  turn_on_at(state, 0.2f);

  state.make_call().set_brightness(1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  wait_a_little(state);
  const float shown = state.get_shown_values().get_brightness();
  ASSERT_GT(shown, 0.2f);
  for (int i = 0; i < 3; i++)  // no loop(): the strip is not updated
    delay(40);
  EXPECT_FLOAT_EQ(state.get_shown_values().get_brightness(), shown);
  wait_a_little(state);
  EXPECT_GT(state.get_shown_values().get_brightness(), shown);
}

TEST(LightMove, MoveFromAnAddressableTurnOnStartsWhereItGot) {
  NullStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  state.loop();

  state.make_call().set_state(true).set_brightness(1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  wait_a_little(state);
  const LightColorValues shown = state.get_shown_values();
  EXPECT_TRUE(shown.is_on());
  EXPECT_GT(shown.get_brightness(), 0.0f);

  auto call = state.make_call();
  call.set_state(true).set_brightness(0.0f).set_transition_length(10000);
  state.perform_move(call, shown);
  EXPECT_EQ(state.current_values, shown);
  wait_a_little(state);
  EXPECT_TRUE(state.current_values.is_on());
  EXPECT_LE(state.current_values.get_brightness(), shown.get_brightness());
  EXPECT_GT(state.current_values.get_brightness(), 0.0f);
}

// Before the strip has drawn a frame of its turn-on, the light is still off, not on at 0%
TEST(LightMove, ShownValuesOfAnAddressableTurnOnBeforeTheFirstFrameAreOff) {
  NullStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  state.loop();

  state.make_call().set_state(true).set_brightness(1.0f).set_transition_length(1000).perform();
  EXPECT_FALSE(state.get_shown_values().is_on());
  wait_a_little(state);
  wait_a_little(state);
  EXPECT_TRUE(state.get_shown_values().is_on());
}

// A transition that replaces the strip's own one starts from its last frame, not from where that one began
TEST(LightMove, ShownValuesOfAnAddressableTransitionThatReplacesAnotherStartWhereItGot) {
  NullStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  turn_on_at(state, 0.2f);

  state.make_call().set_brightness(1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  wait_a_little(state);
  const float shown = state.get_shown_values().get_brightness();
  ASSERT_GT(shown, 0.2f);

  state.make_call().set_brightness(0.9f).set_transition_length(1000).perform();
  EXPECT_FLOAT_EQ(state.get_shown_values().get_brightness(), shown);
}

// On a strip with a buffer, the values shown go on from the last frame of the transition replaced, and follow the LEDs
TEST(LightMove, ShownValuesOfAReplacingAddressableTransitionFollowTheLeds) {
  BufferedStrip strip;
  LightState state(&strip);
  strip.setup_state(&state);
  turn_on_at(state, 0.2f);

  state.make_call().set_brightness(1.0f).set_transition_length(1000).perform();
  for (int i = 0; i < 10; i++)
    wait_a_little(state);
  const float replaced_at = strip.drawn();
  ASSERT_GT(replaced_at, 0.25f);
  EXPECT_NEAR(state.get_shown_values().get_brightness(), replaced_at, 0.02f);

  state.make_call().set_brightness(0.3f).set_transition_length(1000).perform();
  EXPECT_NEAR(state.get_shown_values().get_brightness(), replaced_at, 0.02f);
  for (int i = 0; i < 4; i++) {
    wait_a_little(state);
    const float shown = state.get_shown_values().get_brightness();
    EXPECT_NEAR(shown, strip.drawn(), 0.02f);
    EXPECT_LE(shown, replaced_at + 0.02f);
    EXPECT_GE(shown, 0.3f - 0.02f);
  }
}

TEST(LightMove, MoveToZeroTurnsTheLightOffAtTheEnd) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.5f);

  move_to(state, 0.0f, 20);
  EXPECT_TRUE(state.remote_values.is_on());
  finish(state);
  EXPECT_FALSE(state.current_values.is_on());
  EXPECT_FALSE(state.remote_values.is_on());
}

TEST(LightMove, MoveToZeroNotifiesTargetStateReachedOnce) {
  BrightnessOutput output;
  LightState state(&output);
  ReachedListener listener;
  state.add_target_state_reached_listener(&listener);
  turn_on_at(state, 0.5f);
  listener.reached = 0;

  move_to(state, 0.0f, 20);
  finish(state);
  EXPECT_FALSE(state.remote_values.is_on());
  EXPECT_EQ(listener.reached, 1);
}

TEST(LightMove, CommandFromAListenerAtTheEndOfAMoveToZeroIsKept) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.5f);
  ReachedListener listener(&state);
  state.add_target_state_reached_listener(&listener);

  move_to(state, 0.0f, 20);
  finish(state);
  state.loop();
  EXPECT_TRUE(state.remote_values.is_on());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 0.7f);
  EXPECT_TRUE(state.current_values.is_on());
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), 0.7f);
}

TEST(LightMove, CallAfterAMoveWithoutTransitionIsNotAMove) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.5f);

  auto instant = state.make_call();
  instant.set_brightness(0.4f).set_transition_length(0);
  state.perform_move(instant, state.get_shown_values());
  // A move would turn the light off at the end
  state.make_call().set_state(true).set_brightness(0.0f).set_transition_length(20).perform();
  finish(state);
  EXPECT_TRUE(state.current_values.is_on());
}

TEST(LightMove, CallFromAListenerDuringAMoveIsNotAMove) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.5f);
  TransitionOnPublish listener(&state);
  state.add_remote_values_listener(&listener);

  move_to(state, 1.0f, 10000);
  finish(state);
  EXPECT_TRUE(state.current_values.is_on());
}

// The move is carried by its call: a call made while it performs does not take it, and the move runs and stops
TEST(LightMove, CallMadeWhileTheMoveCallPerformsDoesNotTakeTheMove) {
  CallingOutput output;
  LightState state(&output);
  turn_on_at(state, 0.5f);

  output.state = &state;
  move_to(state, 1.0f, 10000);
  output.state = nullptr;
  ASSERT_TRUE(state.is_moving());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);
  wait_a_little(state);
  state.stop_move();
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_GT(state.current_values.get_brightness(), 0.5f);
  EXPECT_LT(state.current_values.get_brightness(), 1.0f);
}

// The stop freezes the brightness only. Replaying the RGB of a transition's middle through a call would normalise
// it to full brightness: (0.5, 0, 0.5) to (1, 0, 1).
TEST(LightMove, StopOfABrightnessMoveDoesNotReplayTheRgbOfATransition) {
  RgbOutput output;
  LightState state(&output);
  state.make_call().set_state(true).set_rgb(1.0f, 0.0f, 0.0f).set_brightness(0.5f).set_transition_length(0).perform();
  state.loop();
  state.make_call().set_rgb(0.0f, 0.0f, 1.0f).set_transition_length(1000).perform();
  wait_a_little(state);
  ASSERT_GT(output.red, 0.0f);
  ASSERT_GT(output.blue, 0.0f);

  move_to(state, 0.8f, 1000);
  wait_a_little(state);
  state.stop_move();
  state.loop();
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_FLOAT_EQ(output.red, 0.0f);
  EXPECT_FLOAT_EQ(output.blue, state.current_values.get_brightness());
  EXPECT_GT(state.current_values.get_brightness(), 0.5f);
  EXPECT_LT(state.current_values.get_brightness(), 0.8f);
}

// A move to 0% whose end is applied by the next loop(), after its time is up, is stopped where it was last applied
TEST(LightMove, StopAfterTheMoveTimeIsUpFreezesTheLastAppliedValue) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 1.0f);
  move_to(state, 0.0f, 1);
  delay(3);
  state.stop_move();
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_TRUE(state.remote_values.is_on());
  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);
  state.loop();
  EXPECT_FLOAT_EQ(state.current_values.get_brightness(), 1.0f);
}

TEST(LightMove, IsMovingWhileAMoveRuns) {
  BrightnessOutput output;
  LightState state(&output);
  turn_on_at(state, 0.6f);
  EXPECT_FALSE(state.is_moving());
  move_to(state, 1.0f, 1000);
  EXPECT_TRUE(state.is_moving());
  state.stop_move();
  EXPECT_FALSE(state.is_moving());
}

// A move frozen at 0% turns the light off, as the end of a move to 0% does, not leave it on at 0%
TEST(LightMove, StopAtZeroBrightnessTurnsTheLightOff) {
  BrightnessOutput output;
  LightState state(&output);
  state.make_call().set_state(true).set_brightness(0.0f).set_transition_length(0).perform();
  state.loop();
  ASSERT_TRUE(state.current_values.is_on());
  move_to(state, 1.0f, 1);
  delay(3);
  state.stop_move();
  EXPECT_FALSE(state.is_transitioning());
  EXPECT_FALSE(state.remote_values.is_on());
  state.loop();
  EXPECT_FALSE(state.current_values.is_on());
}

}  // namespace esphome::light::testing
