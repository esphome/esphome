#include <gtest/gtest.h>

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

}  // namespace esphome::core::testing
