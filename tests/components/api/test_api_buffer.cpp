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

static const uint8_t BYTES[] = {1, 2, 3, 4, 5, 6};

TEST(APIBuffer, AppendReturnsTheNewBytes) {
  APIBuffer buf;
  ASSERT_TRUE(buf.reserve(8));
  uint8_t *first = buf.append(3);
  ASSERT_NE(first, nullptr);
  std::memcpy(first, BYTES, 3);
  EXPECT_EQ(buf.size(), 3u);
  EXPECT_EQ(buf.capacity(), 8u);

  // Grows through realloc and keeps what was there
  uint8_t *second = buf.append(6);
  ASSERT_EQ(second, buf.data() + 3);
  std::memcpy(second, BYTES + 3, 3);
  EXPECT_EQ(buf.size(), 9u);
  EXPECT_EQ(buf.capacity(), 9u);
  EXPECT_EQ(std::memcmp(buf.data(), BYTES, 6), 0);
}

TEST(APIBuffer, DropFrontSlidesTheRestDown) {
  APIBuffer buf;
  uint8_t *bytes = buf.append(6);
  ASSERT_NE(bytes, nullptr);
  std::memcpy(bytes, BYTES, 6);

  buf.drop_front(2);
  EXPECT_EQ(buf.size(), 4u);
  EXPECT_EQ(buf.capacity(), 6u);
  EXPECT_EQ(std::memcmp(buf.data(), BYTES + 2, 4), 0);

  // Growing afterwards keeps the slid bytes
  ASSERT_TRUE(buf.reserve(64));
  EXPECT_EQ(buf.size(), 4u);
  EXPECT_EQ(std::memcmp(buf.data(), BYTES + 2, 4), 0);

  // Dropping everything leaves an empty buffer with its capacity
  buf.drop_front(4);
  EXPECT_EQ(buf.size(), 0u);
  EXPECT_EQ(buf.capacity(), 64u);
}

}  // namespace esphome::api::testing
