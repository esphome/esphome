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
  int32_t size() const override { return this->buffer_.size() / 4; }
  void clear_effect_data() override {}
  LightTraits get_traits() override { return {}; }
  void write_state(LightState *state) override {}
  // What the light state does before a frame goes out
  void shown() { this->limit_power_(); }
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
  for (bool with_gamma : {false, true}) {
    ESPColorCorrection correction;
    if (with_gamma)
      correction.set_gamma_table(table.data());
    correction.set_power_brightness(128);
    // Below this the gamma curve leaves the halved hardware range too few levels to tell apart
    for (int value = with_gamma ? 96 : 2; value < 256; value++) {
      const int back = correction.color_uncorrect_red(correction.color_correct_red(value));
      EXPECT_LE(std::abs(back - value), with_gamma ? 4 : 2) << "gamma=" << with_gamma << " value=" << value;
    }
  }
}

// A stored level read back and written again must come out unchanged, or effects dim every frame
TEST(PowerLimit, StoredLevelsReEncodeToThemselves) {
  ESPColorCorrection correction;
  correction.set_power_brightness(200);
  for (int logical = 0; logical < 256; logical++) {
    const uint8_t stored = correction.color_correct_red(logical);
    const uint8_t again = correction.color_correct_red(correction.color_uncorrect_red(stored));
    EXPECT_EQ(again, stored) << "logical=" << logical;
  }
}

TEST(PowerLimit, StaysAtFullScaleUnderBudget) {
  Strip strip(10);
  // 10 pixels at 20 mA per channel draw 10 x 60 + 10 mA idle; the budget is twice that
  strip.set_power_estimation(20, 20, 20, 20, 1, 1300);
  strip.all() = Color(255, 255, 255, 0);
  strip.shown();
  EXPECT_EQ(strip.power_brightness(), 255);
  EXPECT_NEAR(strip.get_estimated_current_ma(), 610.0f, 0.5f);
}

TEST(PowerLimit, SettlesOnTheBudgetAndKeepsLogicalLevels) {
  Strip strip(10);
  strip.set_power_estimation(20, 20, 20, 20, 1, 310);  // 300 mA for the pixels of a 600 mA frame
  strip.all() = Color(255, 255, 255, 0);
  strip.shown();
  const uint8_t scale = strip.power_brightness();
  EXPECT_NEAR(scale, 128, 2);
  // The sent frame was rescaled, so a second pass sees the same draw and keeps the scale
  strip.shown();
  EXPECT_EQ(strip.power_brightness(), scale);
  EXPECT_NEAR(strip.get_estimated_current_ma(), 310.0f, 5.0f);
  // Read-back still gives the levels that were written
  EXPECT_NEAR(strip[0].get_red(), 255, 2);
  EXPECT_NEAR(strip[0].get_green(), 255, 2);
}

TEST(PowerLimit, RescalesAFrameWrittenOutsideTheState) {
  Strip strip(4);
  strip.set_power_estimation(20, 20, 20, 20, 1, 124);  // 120 mA for pixels that ask for 240 mA
  strip.all() = Color(255, 255, 255, 0);
  strip.shown();
  const uint8_t scale = strip.power_brightness();
  EXPECT_NEAR(scale, 128, 2);
  for (const auto &view : strip) {
    EXPECT_NEAR(view.get_red_raw(), scale, 1);
    EXPECT_NEAR(view.get_red(), 255, 2);
  }
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
  strip.set_power_estimation(255, 255, 255, 255, 0, 300);
  strip.shown();
  const uint8_t limited = strip.power_brightness();
  ASSERT_LT(limited, 255);
  // Lit channels stay lit while limited
  for (int i = 0; i < 4; i++)
    EXPECT_GE(strip[i].get_green_raw(), 1) << i;
  // A budget the strip cannot reach, and the next frame restores the scale
  strip.set_power_estimation(255, 255, 255, 255, 0, 100000);
  strip.shown();
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
  strip.set_power_estimation(20, 20, 20, 20, 0, 10.04f);  // two full red pixels would draw 40 mA
  strip[0].set_raw(1, 0, 0, 0);
  uint8_t dim = 0;
  for (int cycle = 0; cycle < 4; cycle++) {
    strip[1].set_raw(255, 0, 0, 0);
    strip.shown();
    ASSERT_LT(strip.power_brightness(), 255) << cycle;
    strip[1].set_raw(0, 0, 0, 0);
    strip.shown();
    ASSERT_EQ(strip.power_brightness(), 255) << cycle;
    // The first cycle may move the level by one quantisation step; after that it stays put
    if (cycle > 0)
      EXPECT_EQ(strip[0].get_red_raw(), dim) << cycle;
    dim = strip[0].get_red_raw();
    EXPECT_GE(dim, 1) << cycle;
    EXPECT_LE(dim, 3) << cycle;
  }
}

}  // namespace esphome::light::testing
