#include <gtest/gtest.h>

#include "esphome/components/json/json_util.h"
#include "esphome/components/light/light_call.h"
#include "esphome/components/light/light_json_schema.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

namespace esphome::light::testing {

namespace {

// An `rgbw` light with `color_interlock: true`
class InterlockedRgbwOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    LightTraits traits;
    traits.set_supported_color_modes({ColorMode::RGB, ColorMode::WHITE});
    return traits;
  }
  void write_state(LightState *state) override {}
};

class TestableLightCall : public LightCall {
 public:
  using LightCall::LightCall;
  using LightCall::validate_;
};

LightColorValues parse(LightState &state, const char *payload) {
  TestableLightCall call(&state);
  json::parse_json(payload, [&](JsonObject root) {
    LightJSONSchema::parse_json(state, call, root);
    return true;
  });
  return call.validate_();
}

}  // namespace

// HA's MQTT JSON schema selects the white color mode with a top-level `white` key
TEST(LightJSONSchema, TopLevelWhiteSelectsWhiteMode) {
  InterlockedRgbwOutput output;
  LightState state(&output);
  state.remote_values.set_color_mode(ColorMode::RGB);

  auto values = parse(state, R"({"state":"ON","white":128})");

  EXPECT_EQ(values.get_color_mode(), ColorMode::WHITE);
  EXPECT_FLOAT_EQ(values.get_brightness(), 128.0f / 255.0f);
  EXPECT_FLOAT_EQ(values.get_white(), 1.0f);
}

TEST(LightJSONSchema, TopLevelWhiteOverridesBrightness) {
  InterlockedRgbwOutput output;
  LightState state(&output);

  auto values = parse(state, R"({"state":"ON","brightness":255,"white":128})");

  EXPECT_EQ(values.get_color_mode(), ColorMode::WHITE);
  EXPECT_FLOAT_EQ(values.get_brightness(), 128.0f / 255.0f);
}

TEST(LightJSONSchema, ColorWStillSetsWhite) {
  InterlockedRgbwOutput output;
  LightState state(&output);

  auto values = parse(state, R"({"state":"ON","color":{"w":255}})");

  EXPECT_EQ(values.get_color_mode(), ColorMode::WHITE);
  EXPECT_FLOAT_EQ(values.get_white(), 1.0f);
}

}  // namespace esphome::light::testing
