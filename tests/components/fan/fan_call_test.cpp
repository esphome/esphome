#include <gtest/gtest.h>

#include "esphome/components/fan/fan.h"

namespace esphome::fan::testing {

struct Seen {
  optional<bool> state, oscillating;
  optional<int> speed;
  optional<FanDirection> direction;
  bool operator==(const Seen &) const = default;
};

class CallFan : public Fan {
 public:
  explicit CallFan(int speed_count = 100, bool features = true) : speed_count_(speed_count), features_(features) {}
  FanTraits get_traits() override {
    FanTraits traits(this->features_, this->speed_count_ > 0, this->features_, this->speed_count_);
    this->wire_preset_modes_(traits);
    return traits;
  }
  Seen seen;

 protected:
  void control(const FanCall &call) override {
    this->seen = {call.get_state(), call.get_oscillating(), call.get_speed(), call.get_direction()};
    this->apply_preset_mode_(call);
  }
  int speed_count_;
  bool features_;
};

class ShrinkingFan : public CallFan {
 public:
  bool shrink{false};

 protected:
  void control(const FanCall &call) override {
    if (this->shrink)
      this->set_supported_preset_modes({"auto"});
    CallFan::control(call);
  }
};

TEST(FanCall, SetFieldsReachControl) {
  CallFan fan;
  fan.make_call().set_state(false).set_oscillating(false).set_speed(7).set_direction(FanDirection::FORWARD).perform();
  // false and FORWARD (0) are values, not "unset"
  EXPECT_EQ(fan.seen, (Seen{false, false, 7, FanDirection::FORWARD}));
  fan.make_call().set_state(true).set_oscillating(true).set_direction(FanDirection::REVERSE).perform();
  EXPECT_EQ(fan.seen, (Seen{true, true, 100, FanDirection::REVERSE}));  // turning on picks full speed
}

TEST(FanCall, UnsetFieldsStayUnset) {
  CallFan fan;
  fan.make_call().set_oscillating(optional<bool>{}).set_direction(optional<FanDirection>{}).perform();
  EXPECT_EQ(fan.seen, (Seen{}));
}

TEST(FanCall, SpeedIsClampedToTheSpeedCount) {
  CallFan fan(65535);
  fan.make_call().set_speed(65535).perform();
  EXPECT_EQ(fan.seen.speed, optional<int>(65535));
  fan.make_call().set_speed(70000).perform();
  EXPECT_EQ(fan.seen.speed, optional<int>(65535));
  fan.make_call().set_speed(0).perform();  // below the range, so 1 as before
  EXPECT_EQ(fan.seen.speed, optional<int>(1));
  fan.make_call().set_speed(-5).perform();
  EXPECT_EQ(fan.seen.speed, optional<int>(1));
}

TEST(FanCall, SpeedIsStoredAsGivenUntilPerform) {
  CallFan fan;
  EXPECT_EQ(fan.make_call().set_speed(0).get_speed(), optional<int>(0));
  EXPECT_EQ(fan.make_call().set_speed(-5).get_speed(), optional<int>(-5));
  EXPECT_EQ(fan.make_call().set_speed(70000).get_speed(), optional<int>(70000));
  EXPECT_EQ(fan.make_call().get_speed(), optional<int>{});
}

TEST(FanCall, PresetModeIsKeptByIndex) {
  CallFan fan;
  fan.set_supported_preset_modes({"auto", "sleep"});
  auto call = fan.make_call();
  EXPECT_FALSE(call.has_preset_mode());
  call.set_preset_mode("sleep");
  ASSERT_TRUE(call.has_preset_mode());
  EXPECT_STREQ(call.get_preset_mode(), "sleep");
  call.set_preset_mode("auto");
  EXPECT_STREQ(call.get_preset_mode(), "auto");
  call.set_preset_mode("turbo");  // not supported: cleared, as before
  EXPECT_FALSE(call.has_preset_mode());
  EXPECT_EQ(call.get_preset_mode(), nullptr);
  call.set_preset_mode("sleep").set_preset_mode("");
  EXPECT_FALSE(call.has_preset_mode());
}

TEST(FanCall, PresetModeReachesTheFan) {
  CallFan fan;
  fan.set_supported_preset_modes({"auto", "sleep"});
  fan.make_call().set_preset_mode("sleep").perform();
  EXPECT_EQ(fan.get_preset_mode(), "sleep");
  fan.make_call().set_state(true).perform();  // no preset and no speed: the fan keeps its preset
  EXPECT_EQ(fan.get_preset_mode(), "sleep");
  fan.make_call().set_speed(3).perform();  // a speed clears it
  EXPECT_FALSE(fan.has_preset_mode());
}

// A pending call whose preset index no longer fits the fan's list reads as no preset and leaves the fan's alone
TEST(FanCall, PresetIndexOutsideAReplacedListIsIgnored) {
  CallFan fan;
  fan.set_supported_preset_modes({"auto", "sleep"});
  fan.make_call().set_preset_mode("auto").perform();
  auto call = fan.make_call().set_preset_mode("sleep");
  fan.set_supported_preset_modes({"auto"});
  EXPECT_EQ(call.get_preset_mode(), nullptr);
  call.perform();
  EXPECT_EQ(fan.get_preset_mode(), "auto");
}

// The list shrinking between validate_() and control(), as a log callback could do, leaves the fan's preset alone
TEST(FanCall, PresetListShrunkAfterValidationIsIgnored) {
  ShrinkingFan fan;
  fan.set_supported_preset_modes({"auto", "sleep"});
  fan.make_call().set_preset_mode("auto").perform();
  fan.shrink = true;
  fan.make_call().set_preset_mode("sleep").perform();
  EXPECT_EQ(fan.get_preset_mode(), "auto");
}

// Turning on with a preset that no longer exists still picks full speed, as with no preset
TEST(FanCall, ReplacedPresetDoesNotBlockTheTurnOnSpeed) {
  CallFan fan(3);
  fan.set_supported_preset_modes({"auto", "sleep"});
  auto call = fan.make_call().set_state(true).set_preset_mode("sleep");
  fan.set_supported_preset_modes({"auto"});
  call.perform();
  EXPECT_FALSE(call.has_preset_mode());
  EXPECT_EQ(fan.seen.speed, optional<int>(3));
}

TEST(FanCall, TurningOnWithoutSpeedPicksFullSpeed) {
  CallFan fan(3);
  fan.make_call().set_state(true).perform();
  EXPECT_EQ(fan.seen.speed, optional<int>(3));
}

TEST(FanCall, UnsupportedFeaturesAreDropped) {
  CallFan fan(0, false);
  fan.make_call().set_oscillating(true).set_speed(2).set_direction(FanDirection::REVERSE).perform();
  EXPECT_EQ(fan.seen, (Seen{}));
}

}  // namespace esphome::fan::testing
