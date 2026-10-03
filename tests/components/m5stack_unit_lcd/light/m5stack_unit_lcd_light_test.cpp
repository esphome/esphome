#include "../common.h"

#include "esphome/components/light/light_state.h"
#include "esphome/components/m5stack_unit_lcd/light/m5stack_unit_lcd_light.h"

namespace esphome::m5stack_unit_lcd::testing {

struct LightFixture : public Fixture {
  LightFixture() : state(&this->light) { this->light.set_parent(&this->display); }

  void set(bool on, float brightness) {
    this->state.current_values.set_state(on);
    this->state.current_values.set_brightness(brightness);
    this->light.write_state(&this->state);
  }

  M5StackUnitLCDLight light;
  light::LightState state;
};

TEST(M5StackUnitLCDLight, IsBrightnessOnly) {
  LightFixture f;
  const auto traits = f.light.get_traits();
  EXPECT_TRUE(traits.supports_color_mode(light::ColorMode::BRIGHTNESS));
  EXPECT_FALSE(traits.supports_color_mode(light::ColorMode::RGB));
  EXPECT_FALSE(traits.supports_color_mode(light::ColorMode::ON_OFF));
}

TEST(M5StackUnitLCDLight, WritesBrightnessToTheBacklight) {
  LightFixture f;
  f.boot();
  f.set(true, 0.5f);
  f.set(true, 1.0f);
  f.set(false, 1.0f);  // off -> brightness 0 regardless of the level

  const auto w = f.bus.writes();
  ASSERT_EQ(w.size(), 3u);
  EXPECT_EQ(w[0].written, (std::vector<uint8_t>{CMD_BRIGHTNESS, 128}));
  EXPECT_EQ(w[1].written, (std::vector<uint8_t>{CMD_BRIGHTNESS, 255}));
  EXPECT_EQ(w[2].written, (std::vector<uint8_t>{CMD_BRIGHTNESS, 0}));
}

TEST(M5StackUnitLCDLight, StateWrittenBeforeDisplaySetupIsAppliedAtBoot) {
  // The light sets up before the display (HARDWARE - 1 vs PROCESSOR), so a restored level
  // reaches the display before it has talked to the panel and must be part of the boot config.
  LightFixture f;
  f.set(true, 0.2f);  // -> 51
  EXPECT_TRUE(f.bus.log.empty());
  f.display.run_setup();
  const auto w = f.bus.writes();
  ASSERT_GE(w.size(), 1u);
  EXPECT_EQ(w[0].written,
            (std::vector<uint8_t>{CMD_SET_SLEEP, 0, CMD_SET_POWER, 1, CMD_ROTATE, 0, CMD_BRIGHTNESS, 51, CMD_INVOFF}));
}

}  // namespace esphome::m5stack_unit_lcd::testing
