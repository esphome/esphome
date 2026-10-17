#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <deque>

#include "esphome/components/weikai/weikai.h"

namespace esphome::weikai::testing {

using Ring = WKRingBuffer<uint8_t, 16>;

TEST(WKRingBuffer, PushPopKeepsOrder) {
  Ring rb;
  EXPECT_TRUE(rb.is_empty());
  for (uint8_t i = 0; i < 16; i++)
    EXPECT_TRUE(rb.push(i));
  EXPECT_TRUE(rb.is_full());
  EXPECT_FALSE(rb.push(99));
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t v = 0;
    ASSERT_TRUE(rb.pop(v));
    EXPECT_EQ(v, i);
  }
  uint8_t v = 0;
  EXPECT_FALSE(rb.pop(v));
}

TEST(WKRingBuffer, DirectWriteThenPopBulk) {
  Ring rb;
  EXPECT_EQ(rb.contiguous_free(), 16u);
  for (uint8_t i = 0; i < 10; i++)
    rb.write_ptr()[i] = i;
  rb.commit(10);
  EXPECT_EQ(rb.count(), 10u);
  std::array<uint8_t, 16> out{};
  EXPECT_EQ(rb.pop_bulk(out.data(), 4), 4u);
  for (uint8_t i = 0; i < 4; i++)
    EXPECT_EQ(out[i], i);
  EXPECT_EQ(rb.count(), 6u);
}

TEST(WKRingBuffer, WrapAroundGivesTwoContiguousAreas) {
  Ring rb;
  rb.commit(12);  // head at 12, 12 items stored (content irrelevant here)
  std::array<uint8_t, 16> out{};
  EXPECT_EQ(rb.pop_bulk(out.data(), 8), 8u);  // 4 items left, head 12, tail 8
  EXPECT_EQ(rb.contiguous_free(), 4u);        // only up to the end of the storage
  for (uint8_t i = 0; i < 4; i++)
    rb.write_ptr()[i] = 0xA0 + i;
  rb.commit(4);                         // head wraps to 0
  EXPECT_EQ(rb.contiguous_free(), 8u);  // 8 free slots, from index 0 up to the tail
  for (uint8_t i = 0; i < 8; i++)
    rb.write_ptr()[i] = 0xB0 + i;
  rb.commit(8);
  EXPECT_TRUE(rb.is_full());
  EXPECT_EQ(rb.contiguous_free(), 0u);
  // 4 old items, then 4 + 8 new ones across the wrap point
  EXPECT_EQ(rb.pop_bulk(out.data(), 16), 16u);
  for (uint8_t i = 0; i < 4; i++)
    EXPECT_EQ(out[4 + i], 0xA0 + i);
  for (uint8_t i = 0; i < 8; i++)
    EXPECT_EQ(out[8 + i], 0xB0 + i);
}

TEST(WKRingBuffer, EmptyRestartsAtBeginning) {
  Ring rb;
  rb.commit(5);
  std::array<uint8_t, 16> out{};
  EXPECT_EQ(rb.pop_bulk(out.data(), 5), 5u);
  EXPECT_TRUE(rb.is_empty());
  EXPECT_EQ(rb.contiguous_free(), 16u);  // a full-size bulk write is contiguous again
}

TEST(WKRingBuffer, PopBulkIsLimitedByContent) {
  Ring rb;
  rb.push(1);
  rb.push(2);
  std::array<uint8_t, 16> out{};
  EXPECT_EQ(rb.pop_bulk(out.data(), 10), 2u);
  EXPECT_EQ(rb.pop_bulk(out.data(), 10), 0u);
}

TEST(WKRingBuffer, PeekDoesNotRemove) {
  Ring rb;
  uint8_t v = 0;
  EXPECT_FALSE(rb.peek(v));
  rb.push(7);
  EXPECT_TRUE(rb.peek(v));
  EXPECT_EQ(v, 7);
  EXPECT_EQ(rb.count(), 1u);
}

TEST(WKRingBuffer, RandomOperationsMatchReferenceQueue) {
  Ring rb;
  std::deque<uint8_t> ref;
  uint32_t seed = 12345;
  auto rnd = [&seed]() {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 16;
  };
  uint8_t next = 0;
  for (int step = 0; step < 20000; step++) {
    switch (rnd() % 3) {
      case 0: {  // direct write of a random size
        size_t n = std::min<size_t>(rnd() % 17, rb.contiguous_free());
        for (size_t i = 0; i < n; i++) {
          rb.write_ptr()[i] = next;
          ref.push_back(next++);
        }
        rb.commit(n);
        break;
      }
      case 1: {  // bulk read
        std::array<uint8_t, 16> out{};
        size_t n = rb.pop_bulk(out.data(), rnd() % 17);
        ASSERT_LE(n, ref.size());
        for (size_t i = 0; i < n; i++) {
          ASSERT_EQ(out[i], ref.front());
          ref.pop_front();
        }
        break;
      }
      default: {  // single push or pop
        if (rnd() & 1) {
          if (rb.push(next))
            ref.push_back(next++);
        } else {
          uint8_t v = 0;
          if (rb.pop(v)) {
            ASSERT_EQ(v, ref.front());
            ref.pop_front();
          }
        }
        break;
      }
    }
    ASSERT_EQ(rb.count(), ref.size());
  }
}

}  // namespace esphome::weikai::testing
