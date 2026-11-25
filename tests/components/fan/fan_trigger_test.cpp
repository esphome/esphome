#include <gtest/gtest.h>

#include <cstdio>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "esphome/components/fan/automation.h"
#include "esphome/components/fan/fan.h"

namespace esphome::fan::testing {

// Every trigger firing, in order
static std::vector<std::string> fired;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class TriggerFan : public Fan {
 public:
  TriggerFan() { this->set_supported_preset_modes({"Eco", "Sleep"}); }
  FanTraits get_traits() override {
    FanTraits traits(true, true, true, 3);
    this->wire_preset_modes_(traits);
    return traits;
  }

 protected:
  // Applies the call and publishes, as the template fan does
  void control(const FanCall &call) override {
    if (call.get_state().has_value())
      this->state = *call.get_state();
    if (call.get_speed().has_value())
      this->speed = *call.get_speed();
    if (call.get_oscillating().has_value())
      this->oscillating = *call.get_oscillating();
    if (call.get_direction().has_value())
      this->direction = *call.get_direction();
    this->apply_preset_mode_(call);
    this->publish_state();
  }
};

static TriggerFan *the_fan = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void record_turn_on() { fired.emplace_back("turn_on"); }
static void record_turn_on_again() { fired.emplace_back("turn_on again"); }
static void record_turn_off() { fired.emplace_back("turn_off"); }
static void record(const char *name, int value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s %d", name, value);
  fired.emplace_back(buf);
}
static void record_direction(FanDirection x) { record("direction", static_cast<int>(x)); }
static void record_oscillating(bool x) { record("oscillating", x); }
static void record_speed(int x) { record("speed", x); }
static void record_speed_again(int x) { record("speed again", x); }
static void record_preset(StringRef x) { fired.push_back("preset " + std::string(x.c_str(), x.size())); }

static void turn_on_at_speed_3() {
  record_turn_on();
  the_fan->turn_on().set_speed(3).perform();
}
static void start_oscillating() {
  record_turn_on();
  the_fan->make_call().set_oscillating(true).perform();
}
static void record_speed_then_set_3(int x) {
  record_speed(x);
  the_fan->make_call().set_speed(3).perform();
}
// The on_preset_set automation of tests/components/fan/common.yaml
static void turn_on_for_preset(StringRef x) {
  record_preset(x);
  the_fan->turn_on().set_speed(x.empty() ? 1 : 3).perform();
}

class FanTriggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fired.clear();
    the_fan = &this->fan_;
  }

  // Attach in the order codegen does: on_turn_on, on_turn_off, on_direction_set, on_oscillating_set,
  // on_speed_set, on_preset_set, each key's automations in config order.
  // Each F is one action of the automation, in order.
  template<void (*...F)()> void on_turn_on_() { this->on_edge_<true, F...>(); }
  template<void (*...F)()> void on_turn_off_() { this->on_edge_<false, F...>(); }
  template<void (*...F)(FanDirection)> void on_direction_set_() { this->on_set_<&Fan::direction, F...>(); }
  template<void (*...F)(bool)> void on_oscillating_set_() { this->on_set_<&Fan::oscillating, F...>(); }
  template<void (*...F)(int)> void on_speed_set_() { this->on_set_<&Fan::speed, F...>(); }
  template<void (*...F)(StringRef)> void on_preset_set_() {
    this->fan_.add_on_state_callback([last = this->last_<const char *>()]() {
      if (update_preset_changed(*last, the_fan->get_preset_mode()))
        run<StringRef, F...>(static_cast<StringRef>(the_fan->get_preset_mode()));
    });
  }

  void on_every_change_() {
    this->on_turn_on_<record_turn_on>();
    this->on_turn_off_<record_turn_off>();
    this->on_direction_set_<record_direction>();
    this->on_oscillating_set_<record_oscillating>();
    this->on_speed_set_<record_speed>();
    this->on_preset_set_<record_preset>();
  }

  TriggerFan fan_;

 private:
  // As the generated code: each automation keeps its last value, starting at zero, and its actions get
  // a copy of the value
  template<bool ON, void (*...F)()> void on_edge_() {
    this->fan_.add_on_state_callback([last = this->last_<bool>()]() {
      if (update_changed(*last, the_fan->state) && the_fan->state == ON)
        (F(), ...);
    });
  }
  template<auto Member, auto... F> void on_set_() {
    using T = std::remove_reference_t<decltype(the_fan->*Member)>;
    this->fan_.add_on_state_callback([last = this->last_<T>()]() {
      if (update_changed(*last, the_fan->*Member))
        run<T, F...>(static_cast<T>(the_fan->*Member));
    });
  }
  // A generated automation function, which takes x by reference
  template<typename T, auto... F> static void run(const T &x) { (F(x), ...); }
  template<typename T> T *last_() { return static_cast<T *>(this->lasts_.emplace_back(std::make_shared<T>()).get()); }

  std::vector<std::shared_ptr<void>> lasts_;
};

using Events = std::vector<std::string>;

// The generated change triggers keep their last values in zero-initialized statics, so the fan must start at zero
TEST(FanDefaults, GeneratedChangeTriggersAssumeZeroDefaults) {
  TriggerFan fan;
  EXPECT_FALSE(fan.state);
  EXPECT_FALSE(fan.oscillating);
  EXPECT_EQ(fan.speed, 0);
  EXPECT_EQ(fan.direction, FanDirection::FORWARD);
  EXPECT_EQ(static_cast<int>(FanDirection::FORWARD), 0);
  EXPECT_FALSE(fan.has_preset_mode());
  EXPECT_TRUE(fan.get_preset_mode().empty());
}

TEST_F(FanTriggerTest, FirstPublishWithDefaultStateFiresNothing) {
  this->on_every_change_();
  this->fan_.publish_state();
  EXPECT_EQ(fired, Events{});
}

TEST_F(FanTriggerTest, FirstPublishAfterBootFiresRestoredValues) {
  this->on_every_change_();
  FanRestoreState restored{};
  restored.state = true;
  restored.speed = 2;
  restored.oscillating = true;
  restored.direction = FanDirection::REVERSE;
  restored.preset_mode = 1;
  restored.apply(this->fan_);
  EXPECT_EQ(fired, (Events{"turn_on", "direction 1", "oscillating 1", "speed 2", "preset Sleep"}));
}

TEST_F(FanTriggerTest, NoRefireWithoutChange) {
  this->on_every_change_();
  this->fan_.turn_on().set_speed(2).perform();
  this->fan_.turn_on().set_speed(2).perform();
  this->fan_.publish_state();
  EXPECT_EQ(fired, (Events{"turn_on", "speed 2"}));
}

TEST_F(FanTriggerTest, EachTriggerFiresOnlyForItsOwnField) {
  this->on_every_change_();
  this->fan_.make_call().set_speed(1).perform();
  EXPECT_EQ(fired, Events{"speed 1"});
  fired.clear();
  this->fan_.turn_on().perform();
  EXPECT_EQ(fired, Events{"turn_on"});
  fired.clear();
  this->fan_.make_call().set_direction(FanDirection::REVERSE).perform();
  EXPECT_EQ(fired, Events{"direction 1"});
  fired.clear();
  this->fan_.make_call().set_oscillating(true).perform();
  EXPECT_EQ(fired, Events{"oscillating 1"});
  fired.clear();
  this->fan_.make_call().set_speed(3).perform();
  EXPECT_EQ(fired, Events{"speed 3"});
  fired.clear();
  this->fan_.make_call().set_preset_mode("Eco").perform();
  EXPECT_EQ(fired, Events{"preset Eco"});
  fired.clear();
  this->fan_.turn_off().perform();
  EXPECT_EQ(fired, Events{"turn_off"});
}

// Turning on at speed 0 also sets the top speed
TEST_F(FanTriggerTest, TurnOnAndTurnOffFireOnEdgesOnly) {
  this->on_every_change_();
  this->fan_.turn_off().perform();
  this->fan_.turn_on().perform();
  this->fan_.turn_on().perform();
  this->fan_.turn_off().perform();
  this->fan_.turn_off().perform();
  this->fan_.toggle().perform();
  EXPECT_EQ(fired, (Events{"turn_on", "speed 3", "turn_off", "turn_on"}));
}

TEST_F(FanTriggerTest, SpeedThatClearsPresetFiresEmptyPreset) {
  this->on_every_change_();
  this->fan_.make_call().set_preset_mode("Eco").perform();
  this->fan_.make_call().set_speed(1).perform();
  this->fan_.make_call().set_preset_mode("Eco").perform();
  this->fan_.make_call().set_preset_mode("Sleep").perform();
  EXPECT_EQ(fired, (Events{"preset Eco", "speed 1", "preset ", "preset Eco", "preset Sleep"}));
}

TEST_F(FanTriggerTest, SeveralAutomationsOnOneKeyEachFireOnce) {
  this->on_turn_on_<record_turn_on>();
  this->on_turn_on_<record_turn_on_again>();
  this->on_speed_set_<record_speed>();
  this->on_speed_set_<record_speed_again>();
  this->fan_.turn_on().set_speed(2).perform();
  this->fan_.turn_on().set_speed(2).perform();
  this->fan_.make_call().set_speed(1).perform();
  EXPECT_EQ(fired, (Events{"turn_on", "turn_on again", "speed 2", "speed again 2", "speed 1", "speed again 1"}));
}

TEST_F(FanTriggerTest, ActionChangingTheSameFieldReportsOnlyTheFinalValue) {
  this->on_turn_on_<turn_on_at_speed_3>();
  this->on_speed_set_<record_speed>();
  this->fan_.turn_on().set_speed(1).perform();
  EXPECT_EQ(fired, (Events{"turn_on", "speed 3"}));
}

TEST_F(FanTriggerTest, ActionChangingAnotherFieldKeepsTheOuterChange) {
  this->on_turn_on_<start_oscillating>();
  this->on_oscillating_set_<record_oscillating>();
  this->on_speed_set_<record_speed>();
  this->fan_.turn_on().set_speed(2).perform();
  EXPECT_EQ(fired, (Events{"turn_on", "oscillating 1", "speed 2"}));
}

TEST_F(FanTriggerTest, ActionInFirstOfSeveralAutomationsStillFiresTheOthers) {
  this->on_turn_on_<turn_on_at_speed_3>();
  this->on_turn_on_<record_turn_on_again>();
  this->on_speed_set_<record_speed>();
  this->on_speed_set_<record_speed_again>();
  this->fan_.turn_on().set_speed(1).perform();
  this->fan_.publish_state();
  EXPECT_EQ(fired, (Events{"turn_on", "turn_on again", "speed 3", "speed again 3"}));
}

// x is the value the trigger fired with, also after an earlier action changed the fan
TEST_F(FanTriggerTest, ActionChangingTheSpeedKeepsXForTheNextAction) {
  this->on_speed_set_<record_speed_then_set_3, record_speed_again>();
  this->fan_.make_call().set_speed(1).perform();
  EXPECT_EQ(fired, (Events{"speed 1", "speed 3", "speed again 3", "speed again 1"}));
}

TEST_F(FanTriggerTest, PresetActionThatTurnsOnSettlesAfterClearingThePreset) {
  this->on_speed_set_<record_speed>();
  this->on_preset_set_<turn_on_for_preset>();
  this->fan_.make_call().set_preset_mode("Eco").perform();
  this->fan_.publish_state();
  EXPECT_EQ(fired, (Events{"preset Eco", "speed 3", "preset ", "speed 1"}));
  EXPECT_TRUE(this->fan_.state);
  EXPECT_EQ(this->fan_.speed, 1);
  EXPECT_FALSE(this->fan_.has_preset_mode());
}

}  // namespace esphome::fan::testing
