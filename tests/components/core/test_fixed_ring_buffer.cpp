#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

TEST(FixedRingBufferBulk, PushAndPopInOrder) {
  FixedRingBuffer<uint8_t, 255> buf;
  buf.init(8);
  const uint8_t in[] = {1, 2, 3, 4, 5};
  EXPECT_EQ(buf.push(in, sizeof(in)), 5u);
  EXPECT_EQ(buf.size(), 5);
  EXPECT_EQ(buf.front(), 1);

  std::array<uint8_t, 5> out{};
  EXPECT_EQ(buf.pop(out.data(), out.size()), 5u);
  EXPECT_EQ(out, (std::array<uint8_t, 5>{1, 2, 3, 4, 5}));
  EXPECT_TRUE(buf.empty());
}

TEST(FixedRingBufferBulk, PushStopsWhenFullWithoutOverwriting) {
  FixedRingBuffer<uint8_t, 255> buf;
  buf.init(4);
  const uint8_t first[] = {1, 2, 3};
  const uint8_t second[] = {4, 5, 6};
  EXPECT_EQ(buf.push(first, sizeof(first)), 3u);
  EXPECT_EQ(buf.free_space(), 1);
  EXPECT_EQ(buf.push(second, sizeof(second)), 1u);
  EXPECT_TRUE(buf.full());
  EXPECT_EQ(buf.free_space(), 0);
  EXPECT_EQ(buf.push(second, sizeof(second)), 0u);

  std::array<uint8_t, 4> out{};
  EXPECT_EQ(buf.pop(out.data(), out.size()), 4u);
  EXPECT_EQ(out, (std::array<uint8_t, 4>{1, 2, 3, 4}));
}

TEST(FixedRingBufferBulk, PopFromEmptyReturnsZero) {
  FixedRingBuffer<uint8_t, 255> buf;
  buf.init(4);
  uint8_t out[4] = {9, 9, 9, 9};
  EXPECT_EQ(buf.pop(out, sizeof(out)), 0u);
  EXPECT_EQ(out[0], 9);
}

TEST(FixedRingBufferBulk, PartialPopLeavesTheRest) {
  FixedRingBuffer<uint8_t, 255> buf;
  buf.init(6);
  const uint8_t in[] = {1, 2, 3, 4, 5};
  buf.push(in, sizeof(in));
  uint8_t out[2];
  EXPECT_EQ(buf.pop(out, sizeof(out)), 2u);
  EXPECT_EQ(out[0], 1);
  EXPECT_EQ(out[1], 2);
  EXPECT_EQ(buf.size(), 3);
  EXPECT_EQ(buf.front(), 3);
}

TEST(FixedRingBufferBulk, WrapsAroundTheEnd) {
  FixedRingBuffer<uint8_t, 255> buf;
  buf.init(5);
  const uint8_t lead[] = {10, 11, 12, 13};
  buf.push(lead, sizeof(lead));
  uint8_t drop[3];
  buf.pop(drop, sizeof(drop));  // head 3, tail 4, one element

  // Fills the last slot, then wraps to the start
  const uint8_t in[] = {20, 21, 22, 23};
  EXPECT_EQ(buf.push(in, sizeof(in)), 4u);
  EXPECT_TRUE(buf.full());
  EXPECT_EQ(buf[0], 13);
  EXPECT_EQ(buf[4], 23);

  // Pop crosses the end in two runs
  std::array<uint8_t, 5> out{};
  EXPECT_EQ(buf.pop(out.data(), out.size()), 5u);
  EXPECT_EQ(out, (std::array<uint8_t, 5>{13, 20, 21, 22, 23}));
  EXPECT_TRUE(buf.empty());

  // Indices stay consistent after wrapping, including an exact landing on the end
  EXPECT_EQ(buf.push(in, 3), 3u);
  EXPECT_EQ(buf.pop(out.data(), 3), 3u);
  EXPECT_EQ(out[0], 20);
  EXPECT_EQ(out[2], 22);
  EXPECT_EQ(buf.push(lead, sizeof(lead)), 4u);
  buf.push(lead[0]);
  EXPECT_TRUE(buf.full());
  EXPECT_EQ(buf.pop(out.data(), out.size()), 5u);
  EXPECT_EQ(out, (std::array<uint8_t, 5>{10, 11, 12, 13, 10}));
}

TEST(FixedRingBufferBulk, MixesWithSingleElementCalls) {
  FixedRingBuffer<uint16_t, 1000> buf;
  buf.init(3);
  buf.push(uint16_t{1});
  const uint16_t in[] = {2, 3};
  EXPECT_EQ(buf.push(in, 2), 2u);
  buf.pop();
  EXPECT_TRUE(buf.push(uint16_t{4}));
  uint16_t out[3];
  EXPECT_EQ(buf.pop(out, 3), 3u);
  EXPECT_EQ(out[0], 2);
  EXPECT_EQ(out[1], 3);
  EXPECT_EQ(out[2], 4);
}

TEST(FixedRingBufferBulk, UninitialisedHoldsNothing) {
  FixedRingBuffer<uint8_t, 255> buf;
  const uint8_t in[] = {1};
  uint8_t out[1];
  EXPECT_EQ(buf.push(in, 1), 0u);
  EXPECT_EQ(buf.pop(out, 1), 0u);
}

// A uint8_t index at its limit: head and tail plus a full-buffer length go past 255
TEST(FixedRingBufferBulk, WrapsAtTheIndexTypeLimit) {
  FixedRingBuffer<uint8_t, 255> buf;
  buf.init(255);
  std::array<uint8_t, 255> data{};
  ASSERT_EQ(buf.push(data.data(), 250), 250u);
  ASSERT_EQ(buf.pop(data.data(), 250), 250u);  // head and tail now at 250
  for (size_t i = 0; i < data.size(); i++)
    data[i] = static_cast<uint8_t>(i);
  EXPECT_EQ(buf.push(data.data(), data.size()), 255u);
  EXPECT_TRUE(buf.full());
  EXPECT_EQ(buf[0], 0);
  EXPECT_EQ(buf[254], 254);
  std::array<uint8_t, 255> out{};
  EXPECT_EQ(buf.pop(out.data(), out.size()), 255u);
  EXPECT_EQ(out, data);
  EXPECT_TRUE(buf.empty());
}

template<typename B> static std::vector<int> iterate(B &buf) {
  std::vector<int> seen;
  for (auto &v : buf)
    seen.push_back(v);
  return seen;
}

// Every capacity up to 6, every head position and every fill level: iteration visits the elements oldest first,
// both through a mutable and a const buffer
TEST(FixedRingBufferIterate, VisitsElementsOldestFirst) {
  for (uint8_t capacity = 1; capacity <= 6; capacity++) {
    for (uint8_t head = 0; head < capacity; head++) {
      for (uint8_t count = 0; count <= capacity; count++) {
        FixedRingBuffer<int, 255> buf;
        buf.init(capacity);
        for (uint8_t i = 0; i < head; i++) {
          buf.push(-1);
          buf.pop();
        }
        std::vector<int> expected;
        for (int i = 0; i < count; i++) {
          buf.push(100 + i);
          expected.push_back(100 + i);
        }
        EXPECT_EQ(iterate(buf), expected) << "capacity " << +capacity << " head " << +head << " count " << +count;
        const auto &cbuf = buf;
        EXPECT_EQ(iterate(cbuf), expected) << "capacity " << +capacity << " head " << +head << " count " << +count;
      }
    }
  }
}

TEST(FixedRingBufferIterate, VisitsElementsAfterOverwriting) {
  FixedRingBuffer<int> buf;
  buf.init(3);
  for (int i = 1; i <= 7; i++)
    buf.push_overwrite(i);
  EXPECT_EQ(iterate(buf), (std::vector<int>{5, 6, 7}));
}

TEST(FixedRingBufferIterate, WritesThroughTheIterator) {
  FixedRingBuffer<int> buf;
  buf.init(3);
  for (int i = 1; i <= 4; i++)
    buf.push_overwrite(i);
  for (int &v : buf)
    v *= 10;
  EXPECT_EQ(iterate(buf), (std::vector<int>{20, 30, 40}));
  EXPECT_EQ(buf.front(), 20);
}

TEST(FixedRingBufferIterate, UninitialisedVisitsNothing) {
  FixedRingBuffer<int> buf;
  EXPECT_TRUE(iterate(buf).empty());
  const auto &cbuf = buf;
  EXPECT_TRUE(iterate(cbuf).empty());
}

// A full uint8_t-indexed buffer of 255 elements whose head is not at the start
TEST(FixedRingBufferIterate, VisitsAllAtTheIndexTypeLimit) {
  FixedRingBuffer<uint8_t, 255> buf;
  buf.init(255);
  for (int i = 0; i < 255 + 100; i++)
    buf.push_overwrite(static_cast<uint8_t>(i));
  std::vector<int> expected;
  for (int i = 100; i < 255 + 100; i++)
    expected.push_back(static_cast<uint8_t>(i));
  EXPECT_EQ(iterate(buf), expected);
}

}  // namespace esphome::core::testing
