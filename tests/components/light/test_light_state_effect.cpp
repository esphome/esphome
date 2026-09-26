#include <gtest/gtest.h>

#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

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

class NoopEffect : public LightEffect {
 public:
  using LightEffect::LightEffect;
  void apply() override {}
};

// start_effect_() is where the uint32_t index is narrowed to the stored uint16_t.
class TestableLightState : public LightState {
 public:
  using LightState::LightState;
  using LightState::start_effect_;
};

}  // namespace

// add_effects() is public, so an external component can exceed the codegen cap on effect count;
// an index the uint16_t can't hold must be ignored rather than wrap onto another effect.
TEST(LightStateEffect, IndexAboveUint16IsIgnoredAndKeepsTheActiveEffect) {
  BrightnessOutput output;
  TestableLightState state(&output);
  NoopEffect effect("Noop");
  state.add_effects({&effect});

  state.start_effect_(1);
  ASSERT_EQ(state.get_current_effect_index(), 1u);

  state.start_effect_(0x10000u);  // unchecked narrowing wraps this to 0, which stops the effect
  EXPECT_EQ(state.get_current_effect_index(), 1u);
}

}  // namespace esphome::light::testing
