#include <gtest/gtest.h>

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

}  // namespace

// get_gamma_correct() reads the gamma codegen stores after the lookup table, rounded to two decimals.
TEST(LightStateGamma, ReadsTheGammaStoredWithTheTable) {
  static constexpr GammaTable TABLE{{}, 280};
  BrightnessOutput output;
  LightState state(&output);
  state.set_gamma_table(&TABLE);
  EXPECT_FLOAT_EQ(state.get_gamma_correct(), 2.8f);
  EXPECT_EQ(state.get_gamma_table(), TABLE.lut);
}

TEST(LightStateGamma, IsZeroWithoutATable) {
  BrightnessOutput output;
  LightState state(&output);
  EXPECT_FLOAT_EQ(state.get_gamma_correct(), 0.0f);
  EXPECT_EQ(state.get_gamma_table(), nullptr);
}

}  // namespace esphome::light::testing
