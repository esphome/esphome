#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "esphome/components/light/automation.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/hal.h"

// Pins when the light on_turn_on, on_turn_off and on_state automations run. Each run appends one letter to
// `events_`: N for on_turn_on, F for on_turn_off, S for on_state.

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

class NoopEffect : public LightEffect {
 public:
  using LightEffect::LightEffect;
  void apply() override {}
};

// The only part of these tests that depends on how the automations attach to the light. Codegen always
// attaches every on_turn_on first, then every on_turn_off, then every on_state, so the tests do the same.
class Triggers {
 public:
  explicit Triggers(LightState *state) : state_(state) {}

  void on_turn_on(std::function<void()> action) { this->attach_<LightTurnOnTrigger>(std::move(action)); }
  void on_turn_off(std::function<void()> action) { this->attach_<LightTurnOffTrigger>(std::move(action)); }
  void on_state(std::function<void()> action) { this->attach_<LightStateTrigger>(std::move(action)); }

 protected:
  template<typename T> void attach_(std::function<void()> action) {
    auto trigger = std::make_shared<T>(this->state_);
    auto automation = std::make_shared<Automation<>>(trigger.get());
    auto lambda = std::make_shared<LambdaAction<>>(std::move(action));
    automation->add_action(lambda.get());
    this->keep_.push_back(trigger);
    this->keep_.push_back(automation);
    this->keep_.push_back(lambda);
  }

  LightState *state_;
  std::vector<std::shared_ptr<void>> keep_;
};

class LightTriggersTest : public ::testing::Test {
 protected:
  /// Attach one of each automation, recording into events_.
  void attach_all_() {
    this->triggers_.on_turn_on([this]() { this->events_ += 'N'; });
    this->triggers_.on_turn_off([this]() { this->events_ += 'F'; });
    this->triggers_.on_state([this]() { this->events_ += 'S'; });
  }

  /// Set up the light, off, and drop what that recorded.
  void boot_() {
    this->state_.setup();
    this->state_.loop();
    this->take_();
  }

  /// Run loop() until the running transition or flash ends.
  void finish_transformer_() {
    for (int i = 0; i < 200 && this->state_.is_transitioning(); i++) {
      delay(5);
      this->state_.loop();
    }
    ASSERT_FALSE(this->state_.is_transitioning());
  }

  /// Consume the recorded events.
  std::string take_() {
    std::string events = std::move(this->events_);
    this->events_.clear();
    return events;
  }

  CountingOutput output_;
  LightState state_{&this->output_};
  Triggers triggers_{&this->state_};
  std::string events_;
};

// One of each automation, set up and off
class LightTriggersOffTest : public LightTriggersTest {
 protected:
  void SetUp() override {
    this->attach_all_();
    this->boot_();
  }
};

void start_on(LightStateRTCState &state, bool restored) { state.state = true; }

// What a light saved on at 40% loads at boot
void restore_on(LightStateRTCState &state, bool restored) {
  state.color_mode = ColorMode::BRIGHTNESS;
  state.state = true;
  state.brightness = 0.4f;
}

void restore_on_with_effect(LightStateRTCState &state, bool restored) {
  state.state = true;
  state.effect = 1;
}

}  // namespace

// ---- Boot ----

TEST_F(LightTriggersTest, BootOffRunsOnTurnOffAndOnState) {
  this->attach_all_();
  this->state_.setup();
  EXPECT_EQ(this->take_(), "FS");
  this->state_.loop();
  EXPECT_EQ(this->take_(), "");
}

TEST_F(LightTriggersTest, BootOnRunsOnTurnOnAndOnState) {
  this->attach_all_();
  this->state_.set_state_callback(start_on);
  this->state_.setup();
  EXPECT_EQ(this->take_(), "NS");
  this->state_.loop();
  EXPECT_EQ(this->take_(), "");
}

TEST_F(LightTriggersTest, RestoredOnStateRunsOnTurnOnAtBoot) {
  this->attach_all_();
  this->state_.set_default_transition_length(1000);
  this->state_.set_state_callback(restore_on);
  this->state_.setup();
  EXPECT_EQ(this->take_(), "NS");
  EXPECT_FLOAT_EQ(this->state_.remote_values.get_brightness(), 0.4f);
}

TEST_F(LightTriggersTest, RestoredOnStateWithEffectRunsOnTurnOnOnce) {
  this->attach_all_();
  NoopEffect effect("Noop");
  LightEffect *effects[] = {&effect};
  this->state_.add_effects(effects, 1);
  this->state_.set_state_callback(restore_on_with_effect);
  this->state_.setup();
  EXPECT_EQ(this->take_(), "NS");
  EXPECT_EQ(this->state_.get_current_effect_index(), 1u);
}

TEST_F(LightTriggersTest, FirstPublishBeforeSetupThatIsOnRunsOnTurnOn) {
  this->attach_all_();
  // The automations start from off, whatever the light was before its first report
  this->state_.remote_values.set_state(true);
  this->state_.publish_state();
  EXPECT_EQ(this->take_(), "NS");
}

// ---- Instant changes ----

TEST_F(LightTriggersOffTest, TurnOnRunsOnTurnOnThenOnState) {
  this->state_.turn_on().perform();
  EXPECT_EQ(this->take_(), "NS");
}

TEST_F(LightTriggersOffTest, TurnOffRunsOnTurnOffBeforeOnState) {
  this->state_.turn_on().perform();
  this->take_();
  this->state_.turn_off().perform();
  EXPECT_EQ(this->take_(), "FS");
}

TEST_F(LightTriggersOffTest, BrightnessChangeWhileOnRunsOnlyOnState) {
  this->state_.turn_on().perform();
  this->take_();
  this->state_.turn_on().set_brightness(0.3f).perform();
  EXPECT_EQ(this->take_(), "S");
  this->state_.turn_on().set_brightness(0.6f).perform();
  EXPECT_EQ(this->take_(), "S");
}

TEST_F(LightTriggersOffTest, RepeatedTurnOnRunsOnlyOnState) {
  this->state_.turn_on().perform();
  this->take_();
  this->state_.turn_on().perform();
  EXPECT_EQ(this->take_(), "S");
}

TEST_F(LightTriggersOffTest, RepeatedTurnOffRunsOnTurnOffEachTime) {
  this->state_.turn_off().perform();
  EXPECT_EQ(this->take_(), "FS");
  this->state_.turn_off().perform();
  EXPECT_EQ(this->take_(), "FS");
}

TEST_F(LightTriggersOffTest, ToggleRunsTheMatchingTrigger) {
  this->state_.toggle().perform();
  EXPECT_EQ(this->take_(), "NS");
  this->state_.toggle().perform();
  EXPECT_EQ(this->take_(), "FS");
}

TEST_F(LightTriggersOffTest, ZeroBrightnessTurnOnCountsAsOff) {
  this->state_.turn_on().perform();
  this->take_();
  this->state_.make_call().set_brightness(0.0f).perform();
  this->state_.turn_on().set_brightness(0.5f).perform();
  EXPECT_EQ(this->take_(), "FSNS");
}

TEST_F(LightTriggersOffTest, UnpublishedCallRunsOnlyOnTurnOff) {
  this->state_.turn_on().set_publish(false).perform();
  EXPECT_EQ(this->take_(), "");
  // remote_values was not updated, so the next published turn-on is still a turn-on
  this->state_.turn_on().perform();
  EXPECT_EQ(this->take_(), "NS");
  this->state_.turn_off().set_publish(false).perform();
  EXPECT_EQ(this->take_(), "F");
}

TEST_F(LightTriggersOffTest, PublishStateRunsOnStateEveryTime) {
  this->state_.publish_state();
  this->state_.publish_state();
  EXPECT_EQ(this->take_(), "SS");
  this->state_.turn_on().perform();
  this->take_();
  this->state_.publish_state();
  EXPECT_EQ(this->take_(), "S");
}

// ---- Transitions ----

TEST_F(LightTriggersOffTest, TurnOnTransitionRunsOnTurnOnAtTheStart) {
  this->state_.turn_on().set_transition_length(50).perform();
  EXPECT_EQ(this->take_(), "NS");
  EXPECT_TRUE(this->state_.is_transitioning());
  this->finish_transformer_();
  EXPECT_EQ(this->take_(), "");
}

TEST_F(LightTriggersOffTest, TurnOffTransitionRunsOnTurnOffAtTheEnd) {
  this->state_.turn_on().perform();
  this->take_();
  this->state_.turn_off().set_transition_length(50).perform();
  EXPECT_EQ(this->take_(), "S");
  EXPECT_TRUE(this->state_.is_transitioning());
  this->finish_transformer_();
  EXPECT_EQ(this->take_(), "F");
}

TEST_F(LightTriggersOffTest, BrightnessTransitionWhileOnRunsOnlyOnState) {
  this->state_.turn_on().perform();
  this->take_();
  this->state_.turn_on().set_brightness(0.2f).set_transition_length(50).perform();
  EXPECT_EQ(this->take_(), "S");
  this->finish_transformer_();
  EXPECT_EQ(this->take_(), "");
}

TEST_F(LightTriggersOffTest, TurnOnDuringTurnOffTransitionRunsOnTurnOn) {
  this->state_.turn_on().perform();
  this->take_();
  this->state_.turn_off().set_transition_length(1000).perform();
  this->state_.loop();
  EXPECT_EQ(this->take_(), "S");
  // Reported off already, so the light coming back on is a turn-on although it never went dark
  this->state_.turn_on().set_transition_length(50).perform();
  EXPECT_EQ(this->take_(), "NS");
  this->finish_transformer_();
  EXPECT_EQ(this->take_(), "");
}

TEST_F(LightTriggersOffTest, TurnOffDuringTurnOnTransitionRunsOnTurnOffAtTheEnd) {
  this->state_.turn_on().set_transition_length(1000).perform();
  this->state_.loop();
  EXPECT_EQ(this->take_(), "NS");
  this->state_.turn_off().set_transition_length(50).perform();
  EXPECT_EQ(this->take_(), "S");
  this->finish_transformer_();
  EXPECT_EQ(this->take_(), "F");
}

TEST_F(LightTriggersOffTest, FlashWhileOffRunsOnTurnOffAtBothEnds) {
  // A flash counts as reached at once, while current_values is still off
  this->state_.turn_on().set_flash_length(50).perform();
  EXPECT_EQ(this->take_(), "FNS");
  EXPECT_TRUE(this->state_.is_transitioning());
  // The flash reports its end before the light says it reached the target
  this->finish_transformer_();
  EXPECT_EQ(this->take_(), "SF");
}

// ---- Effects ----

class LightTriggersEffectTest : public LightTriggersTest {
 protected:
  void SetUp() override {
    this->effects_[0] = &this->first_;
    this->effects_[1] = &this->second_;
    this->state_.add_effects(this->effects_, 2);
    this->attach_all_();
    this->boot_();
  }

  NoopEffect first_{"First"};
  NoopEffect second_{"Second"};
  LightEffect *effects_[2]{};
};

TEST_F(LightTriggersEffectTest, TurnOnWithEffectRunsOnTurnOnOnce) {
  this->state_.turn_on().set_effect("First").perform();
  EXPECT_EQ(this->take_(), "NS");
  this->state_.loop();
  this->state_.loop();
  EXPECT_EQ(this->take_(), "");
}

TEST_F(LightTriggersEffectTest, ChangingEffectWhileOnRunsOnlyOnState) {
  this->state_.turn_on().set_effect("First").perform();
  this->take_();
  this->state_.turn_on().set_effect("Second").perform();
  EXPECT_EQ(this->take_(), "S");
  this->state_.turn_on().set_effect("None").perform();
  EXPECT_EQ(this->take_(), "S");
}

TEST_F(LightTriggersEffectTest, TurnOffWithEffectRunsOnTurnOff) {
  this->state_.turn_on().set_effect("First").perform();
  this->take_();
  this->state_.turn_off().perform();
  EXPECT_EQ(this->take_(), "FS");
  EXPECT_EQ(this->state_.get_current_effect_index(), 0u);
}

// ---- Mirrored state ----

TEST_F(LightTriggersOffTest, ExternalStateRunsTheMatchingTrigger) {
  LightColorValues on;
  on.set_color_mode(ColorMode::BRIGHTNESS);
  on.set_state(true);
  on.set_brightness(0.5f);
  EXPECT_TRUE(this->state_.set_external_state(on));
  EXPECT_EQ(this->take_(), "NS");
  on.set_brightness(0.7f);
  EXPECT_TRUE(this->state_.set_external_state(on));
  EXPECT_EQ(this->take_(), "S");
  LightColorValues off = on;
  off.set_state(false);
  EXPECT_TRUE(this->state_.set_external_state(off));
  EXPECT_EQ(this->take_(), "FS");
}

// ---- Automations that change the light ----

TEST_F(LightTriggersTest, OnTurnOnThatTurnsTheLightOffDoesNotLoop) {
  this->triggers_.on_turn_on([this]() {
    this->events_ += 'N';
    this->state_.turn_off().perform();
  });
  this->triggers_.on_turn_off([this]() { this->events_ += 'F'; });
  this->triggers_.on_state([this]() { this->events_ += 'S'; });
  this->boot_();
  this->state_.turn_on().perform();
  // The turn off is reported in full inside on_turn_on, then the outer report finishes
  EXPECT_EQ(this->take_(), "NFSS");
  EXPECT_FALSE(this->state_.remote_values.is_on());
  this->state_.turn_on().perform();
  EXPECT_EQ(this->take_(), "NFSS");
}

TEST_F(LightTriggersTest, LaterOnTurnOnSeesAnEarlierOnesTurnOffOnlyFromInside) {
  this->triggers_.on_turn_on([this]() { this->events_ += 'N'; });
  this->triggers_.on_turn_on([this]() {
    this->events_ += 'n';
    this->state_.turn_off().perform();
  });
  this->triggers_.on_turn_off([this]() { this->events_ += 'F'; });
  this->triggers_.on_state([this]() { this->events_ += 'S'; });
  this->boot_();
  this->state_.turn_on().perform();
  EXPECT_EQ(this->take_(), "NnFSS");
}

TEST_F(LightTriggersTest, OnTurnOnThatTurnsTheLightOffSkipsTheNextOnTurnOn) {
  this->triggers_.on_turn_on([this]() {
    this->events_ += 'N';
    this->state_.turn_off().perform();
  });
  this->triggers_.on_turn_on([this]() { this->events_ += 'n'; });
  this->triggers_.on_turn_off([this]() { this->events_ += 'F'; });
  this->triggers_.on_state([this]() { this->events_ += 'S'; });
  this->boot_();
  this->state_.turn_on().perform();
  // Each on_turn_on keeps its own last state: the second one sees the off report from inside the first, and
  // then the light still off when the outer report reaches it
  EXPECT_EQ(this->take_(), "NFSS");
  this->state_.turn_on().perform();
  EXPECT_EQ(this->take_(), "NFSS");
}

TEST_F(LightTriggersTest, OnTurnOffThatTurnsTheLightOnSkipsTheNextOnTurnOff) {
  bool armed = false;
  this->triggers_.on_turn_off([this, &armed]() {
    this->events_ += 'F';
    if (armed)
      this->state_.turn_on().perform();
  });
  this->triggers_.on_turn_off([this]() { this->events_ += 'f'; });
  this->triggers_.on_state([this]() { this->events_ += 'S'; });
  this->boot_();
  this->state_.turn_on().perform();
  this->take_();
  armed = true;
  this->state_.turn_off().perform();
  // Each on_turn_off tests the light when it runs: the second finds it on again
  EXPECT_EQ(this->take_(), "FSS");
  EXPECT_TRUE(this->state_.remote_values.is_on());
}

TEST_F(LightTriggersTest, OnStateThatTurnsTheLightOnRunsOnTurnOnInside) {
  bool armed = false;
  this->attach_all_();
  this->triggers_.on_state([this, &armed]() {
    if (armed) {
      armed = false;
      this->state_.turn_on().perform();
    }
  });
  this->boot_();
  armed = true;
  this->state_.publish_state();
  EXPECT_EQ(this->take_(), "SNS");
}

TEST_F(LightTriggersTest, EveryAutomationOfAKindRunsInOrder) {
  this->triggers_.on_turn_on([this]() { this->events_ += 'N'; });
  this->triggers_.on_turn_on([this]() { this->events_ += 'n'; });
  this->triggers_.on_turn_off([this]() { this->events_ += 'F'; });
  this->triggers_.on_turn_off([this]() { this->events_ += 'f'; });
  this->triggers_.on_state([this]() { this->events_ += 'S'; });
  this->triggers_.on_state([this]() { this->events_ += 's'; });
  this->boot_();
  this->state_.turn_on().perform();
  EXPECT_EQ(this->take_(), "NnSs");
  this->state_.turn_off().perform();
  EXPECT_EQ(this->take_(), "FfSs");
}

}  // namespace esphome::light::testing
