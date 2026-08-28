#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "esphome/components/light/esp_color_correction.h"

namespace esphome::light::testing {

namespace {

// Mirrors MIN_NONZERO_GAMMA_VALUE in esphome/components/light/__init__.py.
constexpr uint16_t MIN_NONZERO_GAMMA_VALUE = 129;

// Mirrors generate_gamma_table() in esphome/components/light/__init__.py.
std::array<uint16_t, 256> build_gamma_table(double gamma) {
  std::array<uint16_t, 256> table{};
  table[0] = 0;
  for (int i = 1; i < 256; i++) {
    double raw = std::round(std::pow(i / 255.0, gamma) * 65535.0);
    uint16_t clamped = static_cast<uint16_t>(std::min(65535.0, raw));
    table[i] = std::max(MIN_NONZERO_GAMMA_VALUE, clamped);
  }
  return table;
}

// Largest code where the unclamped power curve would still round to 0.
int dead_zone_breakpoint(double gamma) {
  return static_cast<int>(std::ceil(255.0 * std::pow(1.0 / 510.0, 1.0 / gamma)));
}

}  // namespace

// Regression test for esphome/esphome#18842.
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

// Reproduces the reporter's own numbers from esphome/esphome#18842 at gamma=2.8.
TEST(GammaCorrection, DeadZoneFixedAtGamma2_8) {
  const double gamma = 2.8;
  const int n0 = dead_zone_breakpoint(gamma);
  ASSERT_EQ(n0, 28);  // matches the "brightness 28 and above works normally" report

  auto table = build_gamma_table(gamma);
  ESPColorCorrection correction;
  correction.set_gamma_table(table.data());

  for (int i = 1; i < n0; i++) {
    EXPECT_GE(correction.color_correct_red(i), 1) << "index=" << i << " still collapses to 0";
    EXPECT_EQ(table[i], MIN_NONZERO_GAMMA_VALUE) << "index=" << i;
  }

  for (int i = n0; i < 256; i++) {
    double raw = std::round(std::pow(i / 255.0, gamma) * 65535.0);
    uint16_t raw_power_value = static_cast<uint16_t>(std::min(65535.0, raw));
    EXPECT_EQ(table[i], raw_power_value) << "index=" << i << " power curve should be untouched";
  }
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
