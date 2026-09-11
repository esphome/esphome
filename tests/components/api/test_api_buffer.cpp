#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/components/api/api_buffer.h"

namespace esphome::api::testing {

// Pointer plus two 16 bit sizes
static_assert(sizeof(APIBuffer) <= 2 * sizeof(void *));

TEST(APIBuffer, RefusesSizesAbove16Bits) {
  APIBuffer buf;
  ASSERT_TRUE(buf.resize(16));
  EXPECT_FALSE(buf.reserve(UINT16_MAX + 1));
  EXPECT_EQ(buf.size(), 16u);
  EXPECT_EQ(buf.capacity(), 16u);
  EXPECT_TRUE(buf.reserve(UINT16_MAX));
  EXPECT_EQ(buf.capacity(), UINT16_MAX);
}

TEST(APIBuffer, AppendReturnsTheNewBytes) {
  APIBuffer buf;
  uint8_t *first = buf.append(3, 8);
  ASSERT_NE(first, nullptr);
  std::memcpy(first, "abc", 3);
  EXPECT_EQ(buf.size(), 3u);
  EXPECT_EQ(buf.capacity(), 8u);

  uint8_t *second = buf.append(2);
  ASSERT_EQ(second, buf.data() + 3);
  std::memcpy(second, "de", 2);
  EXPECT_EQ(buf.size(), 5u);
  EXPECT_EQ(std::memcmp(buf.data(), "abcde", 5), 0);
}

TEST(APIBuffer, DropFrontKeepsTheRestInPlaceOrWhenGrowing) {
  APIBuffer buf;
  uint8_t *bytes = buf.append(6);
  ASSERT_NE(bytes, nullptr);
  std::memcpy(bytes, "abcdef", 6);

  ASSERT_TRUE(buf.drop_front_and_reserve(2, 6));
  EXPECT_EQ(buf.size(), 4u);
  EXPECT_EQ(buf.capacity(), 6u);
  EXPECT_EQ(std::memcmp(buf.data(), "cdef", 4), 0);

  ASSERT_TRUE(buf.drop_front_and_reserve(1, 64));
  EXPECT_EQ(buf.size(), 3u);
  EXPECT_EQ(buf.capacity(), 64u);
  EXPECT_EQ(std::memcmp(buf.data(), "def", 3), 0);

  EXPECT_FALSE(buf.drop_front_and_reserve(1, UINT16_MAX + 1));
  EXPECT_EQ(buf.size(), 3u);
}

}  // namespace esphome::api::testing
