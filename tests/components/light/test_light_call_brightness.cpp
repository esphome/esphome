#include <gtest/gtest.h>

#include "esphome/components/light/light_call.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

namespace esphome::light::testing {

namespace {

// A light that only supports ON_OFF, like the `binary` platform and `status_led`.
class OnOffOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::ON_OFF});
    return traits;
  }
  void write_state(LightState *state) override {}
};

// A dimmable light, like the `monochromatic` platform.
class BrightnessOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::BRIGHTNESS});
    return traits;
  }
  void write_state(LightState *state) override {}
};

// validate_() is where zero brightness is resolved against the light's capabilities.
class TestableLightCall : public LightCall {
 public:
  using LightCall::LightCall;
  using LightCall::validate_;
};

bool as_binary(const LightColorValues &values) {
  bool binary;
  values.as_binary(&binary);
  return binary;
}

}  // namespace

// An ON/OFF light has no "on but dark" state, so a zero brightness -- how effects encode
// their dark phase -- must turn the light off. Regression test for
// https://github.com/esphome/esphome/issues/17873.
TEST(LightCallOnOff, ZeroBrightnessTurnsOutputOff) {
  OnOffOutput output;
  LightState state(&output);
  TestableLightCall call(&state);

  call.set_state(true).set_brightness(0.0f);
  auto values = call.validate_();

  EXPECT_FALSE(as_binary(values));
}

// The zero must not be stored, or no later turn-on could clear it: the capability check in
// validate_() drops any brightness an ON/OFF light doesn't support, so a stored zero would
// leave the light permanently off.
TEST(LightCallOnOff, ZeroBrightnessIsNotStored) {
  OnOffOutput output;
  LightState state(&output);

  TestableLightCall dark_call(&state);
  dark_call.set_state(true).set_brightness(0.0f);
  state.remote_values = dark_call.validate_();

  EXPECT_FLOAT_EQ(state.remote_values.get_brightness(), 1.0f);

  // A plain turn-on afterwards must switch the light back on.
  TestableLightCall on_call(&state);
  on_call.set_state(true);
  auto values = on_call.validate_();

  EXPECT_TRUE(as_binary(values));
}

// A plain turn-on with no brightness must still light up.
TEST(LightCallOnOff, PlainTurnOnIsVisible) {
  OnOffOutput output;
  LightState state(&output);
  TestableLightCall call(&state);

  call.set_state(true);
  auto values = call.validate_();

  EXPECT_TRUE(as_binary(values));
}

TEST(LightCallOnOff, TurnOffTurnsOutputOff) {
  OnOffOutput output;
  LightState state(&output);
  TestableLightCall call(&state);

  call.set_state(false);
  auto values = call.validate_();

  EXPECT_FALSE(as_binary(values));
}

// A dimmable light can represent "on but dark", so zero brightness must be kept as-is and
// must not be rewritten into a turn-off.
TEST(LightCallBrightness, ZeroBrightnessStaysOnButDark) {
  BrightnessOutput output;
  LightState state(&output);
  TestableLightCall call(&state);

  call.set_state(true).set_brightness(0.0f);
  auto values = call.validate_();

  EXPECT_TRUE(values.is_on());
  EXPECT_FLOAT_EQ(values.get_brightness(), 0.0f);
}

}  // namespace esphome::light::testing
