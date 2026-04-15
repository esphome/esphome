#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

// --- uint32_to_str_unchecked() (internal, raw pointer) ---

TEST(Uint32ToStr, InternalZero) {
  char buf[UINT32_MAX_STR_SIZE];
  char *end = uint32_to_str_unchecked(buf, 0);
  *end = '\0';
  EXPECT_STREQ(buf, "0");
  EXPECT_EQ(end - buf, 1);
}

TEST(Uint32ToStr, InternalSingleDigit) {
  char buf[UINT32_MAX_STR_SIZE];
  char *end = uint32_to_str_unchecked(buf, 7);
  *end = '\0';
  EXPECT_STREQ(buf, "7");
}

TEST(Uint32ToStr, InternalMultiDigit) {
  char buf[UINT32_MAX_STR_SIZE];
  char *end = uint32_to_str_unchecked(buf, 12345);
  *end = '\0';
  EXPECT_STREQ(buf, "12345");
  EXPECT_EQ(end - buf, 5);
}

TEST(Uint32ToStr, InternalMaxValue) {
  char buf[UINT32_MAX_STR_SIZE];
  char *end = uint32_to_str_unchecked(buf, 4294967295u);
  *end = '\0';
  EXPECT_STREQ(buf, "4294967295");
  EXPECT_EQ(end - buf, 10);
}

TEST(Uint32ToStr, InternalPowersOfTen) {
  char buf[UINT32_MAX_STR_SIZE];
  char *end;

  end = uint32_to_str_unchecked(buf, 10);
  *end = '\0';
  EXPECT_STREQ(buf, "10");

  end = uint32_to_str_unchecked(buf, 100);
  *end = '\0';
  EXPECT_STREQ(buf, "100");

  end = uint32_to_str_unchecked(buf, 1000000);
  *end = '\0';
  EXPECT_STREQ(buf, "1000000");
}

// --- uint32_to_str() (public, span API) ---

TEST(Uint32ToStr, SpanZero) {
  char buf[UINT32_MAX_STR_SIZE];
  EXPECT_EQ(uint32_to_str(buf, 0), 1u);
  EXPECT_STREQ(buf, "0");
}

TEST(Uint32ToStr, SpanMultiDigit) {
  char buf[UINT32_MAX_STR_SIZE];
  EXPECT_EQ(uint32_to_str(buf, 12345), 5u);
  EXPECT_STREQ(buf, "12345");
}

TEST(Uint32ToStr, SpanMaxValue) {
  char buf[UINT32_MAX_STR_SIZE];
  EXPECT_EQ(uint32_to_str(buf, 4294967295u), 10u);
  EXPECT_STREQ(buf, "4294967295");
}

}  // namespace esphome::core::testing
