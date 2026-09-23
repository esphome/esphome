#include <gtest/gtest.h>
#include <cmath>
#include "esphome/core/helpers.h"

namespace esphome {

TEST(HelpersTest, Ilog10PowersOfTen) {
  EXPECT_EQ(ilog10(1.0f), 0);
  EXPECT_EQ(ilog10(10.0f), 1);
  EXPECT_EQ(ilog10(100.0f), 2);
  EXPECT_EQ(ilog10(1000.0f), 3);
  EXPECT_EQ(ilog10(10000.0f), 4);
  EXPECT_EQ(ilog10(100000.0f), 5);
  EXPECT_EQ(ilog10(0.1f), -1);
  EXPECT_EQ(ilog10(0.001f), -3);
}

TEST(HelpersTest, Ilog10General) {
  EXPECT_EQ(ilog10(5.0f), 0);
  EXPECT_EQ(ilog10(9.99f), 0);
  EXPECT_EQ(ilog10(50.0f), 1);
  EXPECT_EQ(ilog10(99.0f), 1);
  EXPECT_EQ(ilog10(999.0f), 2);
  EXPECT_EQ(ilog10(0.5f), -1);
  EXPECT_EQ(ilog10(0.0072f), -3);
  EXPECT_EQ(ilog10(120000.0f), 5);
  EXPECT_EQ(ilog10(123456.789f), 5);
}

TEST(HelpersTest, Ilog10Negative) {
  EXPECT_EQ(ilog10(-1.0f), 0);
  EXPECT_EQ(ilog10(-10.0f), 1);
  EXPECT_EQ(ilog10(-0.1f), -1);
  EXPECT_EQ(ilog10(-123.456f), 2);
}

// Verify that ilog10 + pow10_int produces the same rounding result as log10/pow.
// ilog10 may differ from floor(log10f()) for values not exactly representable in float
// (e.g. 0.01f is 0.00999...), but the full round-trip must match.
TEST(HelpersTest, Ilog10RoundTripMatchesLog10) {
  float values[] = {0.0072f, 0.05f,   0.1f,     0.5f,     1.0f,      3.14f,     9.99f, 10.0f, 42.0f,     100.0f,
                    1234.5f, 9999.0f, 10000.0f, 99999.0f, 120000.0f, 999999.0f, -1.0f, -0.1f, -123.456f, -10000.0f};
  for (uint8_t digits = 1; digits <= 6; digits++) {
    for (float v : values) {
      // New implementation using ilog10 + pow10_int
      float factor_new = pow10_int(digits - 1 - ilog10(v));
      float result_new = roundf(v * factor_new) / factor_new;

      // Reference using log10/pow
      double factor_ref = pow(10.0, digits - std::ceil(std::log10(std::fabs(v))));
      float result_ref = static_cast<float>(round(v * factor_ref) / factor_ref);

      EXPECT_FLOAT_EQ(result_new, result_ref) << "mismatch for value=" << v << " digits=" << (int) digits;
    }
  }
}

}  // namespace esphome
