#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>

#include "esphome/components/light/esp_color_correction.h"

namespace esphome::light::testing {

namespace {

// Mirrors generate_gamma_table() in esphome/components/light/__init__.py. The table itself
// is deliberately left as the raw (min-1-clamped) power curve -- ESPColorCorrection is
// responsible for guaranteeing non-zero 8-bit output, not the table.
std::array<uint16_t, 256> build_gamma_table(double gamma) {
  std::array<uint16_t, 256> table{};
  table[0] = 0;
  for (int i = 1; i < 256; i++) {
    double raw = std::round(std::pow(i / 255.0, gamma) * 65535.0);
    table[i] = static_cast<uint16_t>(std::max(1.0, std::min(65535.0, raw)));
  }
  return table;
}

// Mirrors LightState::gamma_correct_lut() in light_state.cpp.
float gamma_correct_lut(const std::array<uint16_t, 256> &table, float value) {
  if (value <= 0.0f)
    return 0.0f;
  if (value >= 1.0f)
    return 1.0f;
  float scaled = value * 255.0f;
  auto idx = static_cast<uint8_t>(scaled);
  if (idx >= 255)
    return table[255] / 65535.0f;
  float frac = scaled - idx;
  float a = table[idx];
  float b = table[idx + 1];
  return (a + frac * (b - a)) / 65535.0f;
}

}  // namespace

// Regression test for esphome/esphome#18842: ESPColorCorrection's own 16-bit -> 8-bit
// conversion must never round a non-zero table entry down to a zero 8-bit output.
TEST(GammaCorrection, NonZeroInputsSurviveConversion) {
  for (double gamma : {1.0, 1.8, 2.0, 2.2, 2.8, 3.0, 4.0}) {
    auto table = build_gamma_table(gamma);
    ESPColorCorrection correction;
    correction.set_gamma_table(table.data());
    for (int i = 1; i < 256; i++) {
      EXPECT_GE(correction.color_correct_red(i), 1) << "gamma=" << gamma << " index=" << i;
    }
  }
}

TEST(GammaCorrection, ZeroInputStaysZero) {
  for (double gamma : {1.0, 2.2, 2.8, 4.0}) {
    auto table = build_gamma_table(gamma);
    ESPColorCorrection correction;
    correction.set_gamma_table(table.data());
    EXPECT_EQ(correction.color_correct_red(0), 0) << "gamma=" << gamma;
  }
}

TEST(GammaCorrection, FullBrightnessStaysFull) {
  for (double gamma : {1.0, 2.2, 2.8, 4.0}) {
    auto table = build_gamma_table(gamma);
    ESPColorCorrection correction;
    correction.set_gamma_table(table.data());
    EXPECT_EQ(correction.color_correct_red(255), 255) << "gamma=" << gamma;
  }
}

// Reproduces the reporter's own numbers from esphome/esphome#18842 at gamma=2.8: codes
// 1-27 previously collapsed to an 8-bit output of 0 and must now be non-zero.
TEST(GammaCorrection, DeadZoneFixedAtGamma2_8) {
  auto table = build_gamma_table(2.8);
  ESPColorCorrection correction;
  correction.set_gamma_table(table.data());
  for (int i = 1; i < 28; i++) {
    EXPECT_GE(correction.color_correct_red(i), 1) << "index=" << i << " still collapses to 0";
  }
}

// Regression test: the 16-bit table itself must stay the untouched power curve. An earlier
// version of this fix raised the table's own floor to survive 8-bit rounding, which also
// flattened the low end of LightState::gamma_correct_lut() -- the float, interpolated path
// shared by every non-addressable (FloatOutput/PWM) light, e.g. LEDC. That path has far more
// than 8-bit precision and was never affected by #18842, so it must see the raw curve.
TEST(GammaCorrection, TableMatchesRawPowerCurve) {
  const double gamma = 2.8;
  auto table = build_gamma_table(gamma);
  for (int i = 1; i < 256; i++) {
    double raw = std::round(std::pow(i / 255.0, gamma) * 65535.0);
    uint16_t expected = static_cast<uint16_t>(std::max(1.0, std::min(65535.0, raw)));
    EXPECT_EQ(table[i], expected) << "index=" << i;
  }

  // Below the 8-bit conversion's dead zone, table values are tiny (not artificially raised),
  // so the interpolated float path still produces distinct output across that range.
  std::set<float> outputs;
  for (int i = 1; i < 28; i++)
    outputs.insert(gamma_correct_lut(table, i / 255.0f));
  EXPECT_GT(outputs.size(), 1u) << "interpolated LUT output should vary, not be flattened";
}

// gamma_table_reverse_search needs a non-decreasing table.
TEST(GammaCorrection, TableStaysNonDecreasing) {
  for (double gamma : {1.0, 1.8, 2.0, 2.2, 2.8, 3.0, 4.0}) {
    auto table = build_gamma_table(gamma);
    for (int i = 1; i < 256; i++) {
      EXPECT_GE(table[i], table[i - 1]) << "gamma=" << gamma << " index=" << i;
    }
  }
}

TEST(GammaCorrection, ReverseSearchFindsLargestIndexLessEqualTarget) {
  auto table = build_gamma_table(2.8);
  for (uint16_t target : {0, 128, 129, 135, 1000, 32768, 65535}) {
    uint8_t lo = gamma_table_reverse_search(table.data(), target);
    EXPECT_LE(table[lo], target) << "target=" << target;
    if (lo < 255) {
      EXPECT_GT(table[lo + 1], target) << "target=" << target;
    }
  }
}

// color_uncorrect_* binary-searches the table via gamma_table_reverse_search().
TEST(GammaCorrection, UncorrectStaysMonotonic) {
  auto table = build_gamma_table(2.8);
  ESPColorCorrection correction;
  correction.set_gamma_table(table.data());

  uint8_t prev = 0;
  for (int i = 1; i < 256; i++) {
    uint8_t result = correction.color_uncorrect_red(i);
    EXPECT_GE(result, prev) << "index=" << i;
    prev = result;
  }
}

}  // namespace esphome::light::testing
