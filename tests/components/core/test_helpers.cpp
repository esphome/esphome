#include <gtest/gtest.h>
#include <cstring>

#include "esphome/core/helpers.h"

namespace esphome::testing {

// --- small_pow10() ---

TEST(SmallPow10, Zero) { EXPECT_EQ(small_pow10(0), 1u); }
TEST(SmallPow10, One) { EXPECT_EQ(small_pow10(1), 10u); }
TEST(SmallPow10, Two) { EXPECT_EQ(small_pow10(2), 100u); }
TEST(SmallPow10, Three) { EXPECT_EQ(small_pow10(3), 1000u); }

// --- uint32_to_str() ---

TEST(Uint32ToStr, Zero) {
  char buf[12];
  char *end = uint32_to_str(buf, 0);
  *end = '\0';
  EXPECT_STREQ(buf, "0");
  EXPECT_EQ(end - buf, 1);
}

TEST(Uint32ToStr, SingleDigit) {
  char buf[12];
  char *end = uint32_to_str(buf, 7);
  *end = '\0';
  EXPECT_STREQ(buf, "7");
}

TEST(Uint32ToStr, MultiDigit) {
  char buf[12];
  char *end = uint32_to_str(buf, 12345);
  *end = '\0';
  EXPECT_STREQ(buf, "12345");
  EXPECT_EQ(end - buf, 5);
}

TEST(Uint32ToStr, Large) {
  char buf[12];
  char *end = uint32_to_str(buf, 4294967295u);
  *end = '\0';
  EXPECT_STREQ(buf, "4294967295");
  EXPECT_EQ(end - buf, 10);
}

TEST(Uint32ToStr, PowersOfTen) {
  char buf[12];
  char *end;

  end = uint32_to_str(buf, 10);
  *end = '\0';
  EXPECT_STREQ(buf, "10");

  end = uint32_to_str(buf, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "100");

  end = uint32_to_str(buf, 1000);
  *end = '\0';
  EXPECT_STREQ(buf, "1000");
}

// --- frac_to_str() ---

TEST(FracToStr, OneDigit) {
  char buf[8];
  char *end = frac_to_str(buf, 5, 1);
  *end = '\0';
  EXPECT_STREQ(buf, "5");
  EXPECT_EQ(end - buf, 1);
}

TEST(FracToStr, TwoDigits) {
  char buf[8];
  char *end = frac_to_str(buf, 46, 10);
  *end = '\0';
  EXPECT_STREQ(buf, "46");
}

TEST(FracToStr, ThreeDigits) {
  char buf[8];
  char *end = frac_to_str(buf, 456, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "456");
  EXPECT_EQ(end - buf, 3);
}

TEST(FracToStr, LeadingZeros) {
  char buf[8];
  char *end = frac_to_str(buf, 1, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "001");

  end = frac_to_str(buf, 5, 10);
  *end = '\0';
  EXPECT_STREQ(buf, "05");
}

TEST(FracToStr, AllZeros) {
  char buf[8];
  char *end = frac_to_str(buf, 0, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "000");

  end = frac_to_str(buf, 0, 1);
  *end = '\0';
  EXPECT_STREQ(buf, "0");
}

TEST(FracToStr, ZeroDivisor) {
  char buf[8];
  buf[0] = 'X';
  char *end = frac_to_str(buf, 0, 0);
  EXPECT_EQ(end, buf);  // writes nothing
}

}  // namespace esphome::testing
