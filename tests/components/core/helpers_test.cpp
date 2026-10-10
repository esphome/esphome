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

TEST(StaticVectorTest, ConvertingConstructorFromSmaller) {
  StaticVector<uint8_t, 5> small{0x03, 0x00, 0x10, 0x00, 0x01};
  StaticVector<uint8_t, 253> big = small;
  ASSERT_EQ(big.size(), small.size());
  for (size_t i = 0; i < small.size(); i++) {
    EXPECT_EQ(big[i], small[i]) << "mismatch at index " << i;
  }
}

TEST(StaticVectorTest, ConvertingConstructorPartiallyFilledAndEmpty) {
  StaticVector<uint8_t, 5> partial{0xAA, 0xBB};
  StaticVector<uint8_t, 8> from_partial = partial;
  ASSERT_EQ(from_partial.size(), 2u);
  EXPECT_EQ(from_partial[0], 0xAA);
  EXPECT_EQ(from_partial[1], 0xBB);

  StaticVector<uint8_t, 5> empty;
  StaticVector<uint8_t, 8> from_empty = empty;
  EXPECT_TRUE(from_empty.empty());
}

TEST(StaticVectorTest, ConvertingConstructorSameSize) {
  StaticVector<int, 3> src{1, 2, 3};
  StaticVector<int, 3> dst = src;
  ASSERT_EQ(dst.size(), 3u);
  EXPECT_EQ(dst[2], 3);
}

TEST(StringContainsIgnoreCaseTest, NullPointerAlwaysFalse) {
  const char *haystack = nullptr;
  const char *needle = nullptr;

  EXPECT_FALSE(str_contains_ignore_case(haystack, needle));
  EXPECT_FALSE(str_contains_ignore_case("Hello World", needle));
  EXPECT_FALSE(str_contains_ignore_case(haystack, "anything"));
}

TEST(StringContainsIgnoreCaseTest, EmptySearchMatches) {
  const char *haystack = "Hello World";

  EXPECT_TRUE(str_contains_ignore_case_fallback(haystack, ""));
}

TEST(StringContainsIgnoreCaseTest, MiscCaseMatches) {
  const char *haystack = "Hello World";

  EXPECT_TRUE(str_contains_ignore_case_fallback(haystack, "Hello"));
  EXPECT_TRUE(str_contains_ignore_case_fallback(haystack, "hello"));
  EXPECT_TRUE(str_contains_ignore_case_fallback(haystack, "HELLO"));
  EXPECT_TRUE(str_contains_ignore_case_fallback(haystack, "hELLO"));
}

TEST(StringContainsIgnoreCaseTest, MiscNotMatching) {
  const char *haystack = "Hello World";

  // Expected to match
  EXPECT_TRUE(str_contains_ignore_case_fallback(haystack, "Hell"));

  // Expected not to match
  EXPECT_FALSE(str_contains_ignore_case_fallback(haystack, "Heaven"));
  EXPECT_FALSE(str_contains_ignore_case_fallback(haystack, "Hello!"));
  EXPECT_FALSE(str_contains_ignore_case_fallback(haystack, "world!"));
}

TEST(StringContainsIgnoreCaseTest, FallbackMatchesLibc) {
  const char *haystack = "Hello World";
  for (const char *needle : {"", "Hello", "hELLO", "HELLO", "Hell", "world", "World", "Heaven", "Hello!", "d"}) {
    EXPECT_EQ(str_contains_ignore_case_fallback(haystack, needle), str_contains_ignore_case(haystack, needle))
        << "needle: " << needle;
  }
  EXPECT_EQ(str_contains_ignore_case_fallback("", ""), str_contains_ignore_case("", ""));
  EXPECT_EQ(str_contains_ignore_case_fallback("ab", "abc"), str_contains_ignore_case("ab", "abc"));
}

}  // namespace esphome
