#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "esphome/components/light/esp_color_correction.h"

namespace esphome::light::testing {

namespace {

// A representative fixture for ESPColorCorrection/gamma_table_reverse_search tests below --
// not a spec for generate_gamma_table() itself, which the Python tests own.
std::array<uint16_t, 256> build_gamma_table(double gamma) {
  std::array<uint16_t, 256> table{};
  table[0] = 0;
  for (int i = 1; i < 256; i++) {
    double raw = std::round(std::pow(i / 255.0, gamma) * 65535.0);
    table[i] = static_cast<uint16_t>(std::max(1.0, std::min(65535.0, raw)));
  }
  return table;
}

// Bundles a table with an ESPColorCorrection pointing at it, since the correction only holds
// a raw pointer into the table and doesn't own it.
struct GammaFixture {
  explicit GammaFixture(double gamma) : table(build_gamma_table(gamma)) { correction.set_gamma_table(table.data()); }
  std::array<uint16_t, 256> table;
  ESPColorCorrection correction;
};

}  // namespace

// Regression test for esphome/esphome#18842: ESPColorCorrection's own 16-bit -> 8-bit
// conversion must never round a non-zero table entry down to a zero 8-bit output.
TEST(GammaCorrection, NonZeroInputsSurviveConversion) {
  for (double gamma : {1.0, 1.8, 2.0, 2.2, 2.8, 3.0, 4.0}) {
    GammaFixture fixture(gamma);
    for (int i = 1; i < 256; i++) {
      EXPECT_GE(fixture.correction.color_correct_red(i), 1) << "gamma=" << gamma << " index=" << i;
    }
  }
}

TEST(GammaCorrection, ZeroInputStaysZero) {
  for (double gamma : {1.0, 2.2, 2.8, 4.0}) {
    GammaFixture fixture(gamma);
    EXPECT_EQ(fixture.correction.color_correct_red(0), 0) << "gamma=" << gamma;
  }
}

TEST(GammaCorrection, FullBrightnessStaysFull) {
  for (double gamma : {1.0, 2.2, 2.8, 4.0}) {
    GammaFixture fixture(gamma);
    EXPECT_EQ(fixture.correction.color_correct_red(255), 255) << "gamma=" << gamma;
  }
}

// Reproduces the reporter's own numbers from esphome/esphome#18842 at gamma=2.8: codes
// 1-27 previously collapsed to an 8-bit output of 0 and must now be non-zero.
TEST(GammaCorrection, DeadZoneFixedAtGamma28) {
  GammaFixture fixture(2.8);
  for (int i = 1; i < 28; i++) {
    EXPECT_GE(fixture.correction.color_correct_red(i), 1) << "index=" << i << " still collapses to 0";
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
  GammaFixture fixture(2.8);
  uint8_t prev = 0;
  for (int i = 1; i < 256; i++) {
    uint8_t result = fixture.correction.color_uncorrect_red(i);
    EXPECT_GE(result, prev) << "index=" << i;
    prev = result;
  }
}

}  // namespace esphome::light::testing
