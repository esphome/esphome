#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

template<typename T, size_t N> static std::vector<T> drain(StaticRingBuffer<T, N> &buf) {
  std::vector<T> out;
  while (!buf.empty()) {
    out.push_back(buf.front());
    buf.pop();
  }
  return out;
}

TEST(StaticRingBufferPushFront, PoppedFirst) {
  StaticRingBuffer<int, 4> buf;
  buf.push(1);
  buf.push(2);
  EXPECT_TRUE(buf.push_front(0));
  EXPECT_EQ(buf.size(), 3);
  EXPECT_EQ(drain(buf), (std::vector<int>{0, 1, 2}));
}

TEST(StaticRingBufferPushFront, WrapsBelowZeroAndRejectsWhenFull) {
  StaticRingBuffer<int, 4> buf;
  EXPECT_TRUE(buf.push_front(3));
  EXPECT_TRUE(buf.push_front(2));
  EXPECT_TRUE(buf.push_front(1));
  EXPECT_TRUE(buf.push(4));
  EXPECT_FALSE(buf.push_front(0));
  EXPECT_EQ(drain(buf), (std::vector<int>{1, 2, 3, 4}));
}

TEST(StaticRingBufferPushFront, AfterTailWraps) {
  StaticRingBuffer<int, 4> buf;
  buf.push(1);
  buf.push(2);
  buf.push(3);
  buf.pop();
  buf.pop();
  buf.push(4);
  buf.push(5);  // tail wrapped to index 1, head at 2
  EXPECT_TRUE(buf.push_front(0));
  EXPECT_EQ(drain(buf), (std::vector<int>{0, 3, 4, 5}));
}

TEST(StaticRingBufferBulk, PushStopsWhenFullAndPopsInOrder) {
  StaticRingBuffer<uint8_t, 4> buf;
  const uint8_t in[] = {1, 2, 3, 4, 5, 6};
  EXPECT_EQ(buf.push(in, 3), 3u);
  EXPECT_EQ(buf.free_space(), 1);
  EXPECT_EQ(buf.push(in + 3, 3), 1u);
  EXPECT_EQ(buf.free_space(), 0);
  std::array<uint8_t, 4> out{};
  EXPECT_EQ(buf.pop(out.data(), out.size()), 4u);
  EXPECT_EQ(out, (std::array<uint8_t, 4>{1, 2, 3, 4}));
  EXPECT_EQ(buf.pop(out.data(), out.size()), 0u);
}

TEST(StaticRingBufferBulk, WrapsAroundTheEnd) {
  StaticRingBuffer<uint8_t, 5> buf;
  const uint8_t lead[] = {10, 11, 12, 13};
  buf.push(lead, sizeof(lead));
  uint8_t drop[3];
  buf.pop(drop, sizeof(drop));  // head 3, tail 4, one element
  const uint8_t in[] = {20, 21, 22, 23};
  EXPECT_EQ(buf.push(in, sizeof(in)), 4u);
  std::array<uint8_t, 5> out{};
  EXPECT_EQ(buf.pop(out.data(), out.size()), 5u);
  EXPECT_EQ(out, (std::array<uint8_t, 5>{13, 20, 21, 22, 23}));
  EXPECT_TRUE(buf.empty());
}

}  // namespace esphome::core::testing
