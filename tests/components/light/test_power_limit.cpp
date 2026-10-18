#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/esp_color_correction.h"

namespace esphome::light::testing {

namespace {

std::array<uint16_t, 256> build_gamma_table(double gamma) {
  std::array<uint16_t, 256> table{};
  for (int i = 1; i < 256; i++) {
    double raw = std::round(std::pow(i / 255.0, gamma) * 65535.0);
    table[i] = static_cast<uint16_t>(std::max(1.0, std::min(65535.0, raw)));
  }
  return table;
}

// A strip of RGBW pixels in memory, with the protected hooks the tests drive
class Strip : public AddressableLight {
 public:
  explicit Strip(int32_t count) : buffer_(count * 4) {}
  ~Strip() { delete this->power_estimation_; }  // NOLINT(cppcoreguidelines-owning-memory)
  int32_t size() const override { return this->buffer_.size() / 4; }
  void clear_effect_data() override {}
  LightTraits get_traits() override { return {}; }
  void write_state(LightState *state) override { this->sent_red = (*this)[0].get_red_raw(); }
  uint8_t sent_red{0};
  // Every channel draws ma_per_channel at full level; setup turns the settings into the idle draw and budget
  void configure_power(float ma_per_channel, float idle_ma_per_led, float max_current_ma) {
    this->set_power_estimation(ma_per_channel, ma_per_channel, ma_per_channel, ma_per_channel, idle_ma_per_led);
    this->set_max_current_ma(max_current_ma);
    this->call_setup();
  }
  // What the light state does before a frame goes out
  void limit() { this->limit_power_(); }
  uint8_t power_brightness() const { return this->correction_.get_power_brightness(); }

 protected:
  ESPColorView get_view_internal(int32_t index) const override {
    auto *pixel = const_cast<uint8_t *>(&this->buffer_[index * 4]);
    return {pixel, pixel + 1, pixel + 2, pixel + 3, nullptr, &this->correction_};
  }

  std::vector<uint8_t> buffer_;
};

}  // namespace

TEST(PowerLimit, CorrectionRoundTripsUnderTheLimit) {
  const auto table = build_gamma_table(2.2);
  for (uint8_t scale : {128, 200}) {
    for (bool with_gamma : {false, true}) {
      ESPColorCorrection correction;
      if (with_gamma)
        correction.set_gamma_table(table.data());
      correction.set_power_brightness(scale);
      for (int value = 0; value < 256; value++) {
        const uint8_t stored = correction.color_correct_red(value);
        const int back = correction.color_uncorrect_red(stored);
        // A read, change-nothing and write re-encodes to the stored level, so the dimming never compounds
        EXPECT_EQ(correction.color_correct_red(back), stored)
            << "scale=" << int(scale) << " gamma=" << with_gamma << " value=" << value;
        // Below 96 the gamma curve leaves the limited hardware range too few levels to tell apart
        if (with_gamma ? value >= 96 : value >= 2) {
          EXPECT_LE(std::abs(back - value), with_gamma ? 4 : 2)
              << "scale=" << int(scale) << " gamma=" << with_gamma << " value=" << value;
        }
      }
    }
  }
}

TEST(PowerLimit, StaysAtFullScaleUnderBudget) {
  Strip strip(10);
  // 10 pixels at 20 mA per channel draw 10 x 60 + 10 mA idle; the budget is twice that
  strip.configure_power(20, 1, 1300);
  strip.all() = Color(255, 255, 255, 0);
  strip.limit();
  EXPECT_EQ(strip.power_brightness(), 255);
  EXPECT_NEAR(strip.get_estimated_current_ma(), 610.0f, 0.5f);
}

TEST(PowerLimit, SettlesOnTheBudgetAndKeepsLogicalLevels) {
  Strip strip(10);
  strip.configure_power(20, 1, 310);  // 300 mA for the pixels of a 600 mA frame
  strip.all() = Color(255, 255, 255, 0);
  strip.limit();
  const uint8_t scale = strip.power_brightness();
  EXPECT_NEAR(scale, 128, 2);
  // The estimate is of the frame that goes out, not of what was asked for
  EXPECT_NEAR(strip.get_estimated_current_ma(), 310.0f, 5.0f);
  // The sent frame was rescaled, so a second pass sees the same draw and keeps the scale
  strip.limit();
  EXPECT_EQ(strip.power_brightness(), scale);
  EXPECT_NEAR(strip.get_estimated_current_ma(), 310.0f, 5.0f);
  // Read-back still gives the levels that were written
  EXPECT_NEAR(strip[0].get_red(), 255, 2);
  EXPECT_NEAR(strip[0].get_green(), 255, 2);
}

TEST(PowerLimit, APixelWrittenUnderTheLimitLandsLimited) {
  Strip strip(4);
  strip.configure_power(20, 1, 124);  // 120 mA for pixels that ask for 240 mA
  strip.all() = Color(255, 255, 255, 0);
  strip.limit();
  const uint8_t scale = strip.power_brightness();
  ASSERT_LT(scale, 255);
  // A single pixel rewritten by a lambda lands at the limited level straight away
  strip[1] = Color(128, 0, 0, 0);
  EXPECT_NEAR(strip[1].get_red_raw(), (128 * scale) / 255.0f, 2);
  EXPECT_NEAR(strip[1].get_red(), 128, 2);
}

TEST(PowerLimit, LevelsDoNotCreepOverALimitCycle) {
  Strip strip(4);
  // Levels written once and never touched again, as a lambda would leave them
  for (int i = 0; i < 4; i++)
    strip[i].set_raw(1 + i * 40, 2, 100, 0);
  // 1020 mA at full level, so the budget of 300 mA limits the strip
  strip.configure_power(255, 0, 300);
  strip.limit();
  const uint8_t limited = strip.power_brightness();
  ASSERT_LT(limited, 255);
  // Lit channels stay lit while limited
  for (int i = 0; i < 4; i++)
    EXPECT_GE(strip[i].get_green_raw(), 1) << i;
  // A budget the strip cannot reach, and the next frame restores the scale
  strip.configure_power(255, 0, 100000);
  strip.limit();
  ASSERT_EQ(strip.power_brightness(), 255);
  // Each level comes back within one quantisation step of where it started, never below it
  const int step = (255 + limited - 1) / limited;
  for (int i = 0; i < 4; i++) {
    const int red = 1 + i * 40;
    EXPECT_GE(strip[i].get_red_raw(), red) << i;
    EXPECT_LE(strip[i].get_red_raw(), red + step) << i;
    EXPECT_GE(strip[i].get_blue_raw(), 100) << i;
    EXPECT_LE(strip[i].get_blue_raw(), 100 + step) << i;
  }
}

TEST(PowerLimit, ADimPixelSettlesWhileDemandAlternates) {
  Strip strip(2);
  strip.configure_power(20, 0, 10.04f);  // two full red pixels would draw 40 mA
  strip[0].set_raw(1, 0, 0, 0);
  uint8_t dim = 0;
  for (int cycle = 0; cycle < 4; cycle++) {
    strip[1].set_raw(255, 0, 0, 0);
    strip.limit();
    ASSERT_LT(strip.power_brightness(), 255) << cycle;
    strip[1].set_raw(0, 0, 0, 0);
    strip.limit();
    ASSERT_EQ(strip.power_brightness(), 255) << cycle;
    // The first cycle may move the level by one quantisation step; after that it stays put
    if (cycle > 0)
      EXPECT_EQ(strip[0].get_red_raw(), dim) << cycle;
    dim = strip[0].get_red_raw();
    EXPECT_GE(dim, 1) << cycle;
    EXPECT_LE(dim, 3) << cycle;
  }
}

TEST(PowerLimit, TheStateLimitsAFrameBeforeTheDriverSendsIt) {
  Strip strip(2);
  AddressableLightState state(&strip);
  strip.setup_state(&state);
  strip.configure_power(20, 0, 10);  // two full red pixels would draw 40 mA
  strip.all() = Color(255, 0, 0, 0);
  strip.schedule_show();
  state.loop();
  EXPECT_LT(strip.sent_red, 255);
  EXPECT_EQ(strip.sent_red, strip[0].get_red_raw());
  // Nothing pending: the next loop neither limits nor writes
  strip[0].set_raw(255, 0, 0, 0);
  state.loop();
  EXPECT_EQ(strip[0].get_red_raw(), 255);
}

TEST(PowerLimit, ALevelPastTheScaleInForceSaturatesWhenTheScaleRises) {
  Strip strip(2);
  strip.configure_power(20, 0, 30);  // two full red pixels would draw 40 mA
  strip.all() = Color(255, 0, 0, 0);
  strip.limit();
  ASSERT_LT(strip.power_brightness(), 255);
  // A raw write past the scale, as a lambda or a partition over another strip can make; one such pixel
  // alone fits the budget, so the scale goes back up
  strip[0].set_raw(255, 0, 0, 0);
  strip[1].set_raw(0, 0, 0, 0);
  strip.limit();
  ASSERT_EQ(strip.power_brightness(), 255);
  EXPECT_EQ(strip[0].get_red_raw(), 255);
}

TEST(PowerLimit, RepeatedPassesOverAnUnchangedFrameHoldTheScale) {
  Strip strip(1);
  strip.configure_power(20, 0, 5);  // a half-red pixel asks for 10 mA
  strip[0] = Color(128, 0, 0, 0);
  strip.limit();
  const uint8_t scale = strip.power_brightness();
  EXPECT_NEAR(scale, 127, 2);
  // A driver retry re-runs the pass with nothing rewritten: the scale and the read-back stay put
  for (int pass = 0; pass < 20; pass++)
    strip.limit();
  EXPECT_EQ(strip.power_brightness(), scale);
  EXPECT_NEAR(strip[0].get_red(), 128, 2);
}

TEST(PowerLimit, AHeavyLimitStaysWithinOneStepOfTheBudget) {
  Strip strip(1);
  strip.configure_power(20, 0, 0.3f);  // a full red pixel asks for 20 mA against a 0.3 mA budget
  strip[0] = Color(255, 0, 0, 0);
  for (int pass = 0; pass < 20; pass++)
    strip.limit();
  const uint8_t scale = strip.power_brightness();
  EXPECT_GE(scale, 1);
  EXPECT_LE(scale, 4);
  // No dead band this low: the frame is within one level of the budget
  EXPECT_LE(strip.get_estimated_current_ma(), 0.3f + 20.0f / 255.0f);
}

TEST(PowerLimit, AChangedFrameTakesTheDropAtALowScale) {
  Strip strip(10);
  strip.configure_power(20, 1, 20);  // white pixels draw 60 mA each against 10 mA left for the pixels
  for (int i = 0; i < 5; i++)
    strip[i] = Color(255, 255, 255, 0);
  strip.limit();
  const uint8_t five = strip.power_brightness();
  EXPECT_NEAR(five, 8, 1);
  // Twice the pixels lit: well past one level per channel over the budget, so the scale drops
  strip.all() = Color(255, 255, 255, 0);
  strip.limit();
  const uint8_t ten = strip.power_brightness();
  EXPECT_LT(ten, five);
  // A retry with nothing rewritten then holds it
  strip.limit();
  EXPECT_EQ(strip.power_brightness(), ten);
}

TEST(PowerLimit, AChangingPixelOverAStaticBackgroundDoesNotWalkTheScaleDown) {
  Strip strip(100);
  strip.configure_power(20, 0, 60);  // 99 pixels at red 100 ask for 776 mA
  for (int i = 1; i < 100; i++)
    strip[i] = Color(100, 0, 0, 0);
  strip[0] = Color(255, 0, 0, 0);
  strip.limit();
  const uint8_t first = strip.power_brightness();
  EXPECT_NEAR(first, 19, 2);
  // A dot blinks over the background: every frame differs, so every pass runs
  for (int pass = 0; pass < 40; pass++) {
    strip[0] = Color(pass % 2 ? 255 : 0, 0, 0, 0);
    strip.limit();
  }
  EXPECT_NEAR(strip.power_brightness(), first, 2);
  // One raw level at this scale is about 13 logical levels
  EXPECT_NEAR(strip[50].get_red(), 100, 16);
}

}  // namespace esphome::light::testing
