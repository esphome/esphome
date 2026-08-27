// Exercises the no-atomics LockFreeQueue implementation (PlainAtomic indices —
// the path used on cores without atomic RMW instructions, currently BK72xx).
// The define is forced before the include so this TU deterministically compiles
// that path regardless of the host's default thread model; no other test TU
// instantiates this template, so the differing definition is confined here.
#define ESPHOME_THREAD_MULTI_NO_ATOMICS
#include "esphome/core/lock_free_queue.h"

#include <gtest/gtest.h>

namespace esphome::core::testing {

TEST(LockFreeQueueNoAtomics, EmptyPopReturnsNull) {
  esphome::LockFreeQueue<int, 4> q;
  EXPECT_EQ(q.pop(), nullptr);
  EXPECT_TRUE(q.empty());
  EXPECT_FALSE(q.full());
  EXPECT_EQ(q.size(), 0u);
}

TEST(LockFreeQueueNoAtomics, FifoOrder) {
  esphome::LockFreeQueue<int, 4> q;
  int a = 1, b = 2, c = 3;
  EXPECT_TRUE(q.push(&a));
  EXPECT_TRUE(q.push(&b));
  EXPECT_TRUE(q.push(&c));
  EXPECT_EQ(q.size(), 3u);
  EXPECT_EQ(q.pop(), &a);
  EXPECT_EQ(q.pop(), &b);
  EXPECT_EQ(q.pop(), &c);
  EXPECT_EQ(q.pop(), nullptr);
}

TEST(LockFreeQueueNoAtomics, CapacityIsSizeMinusOne) {
  esphome::LockFreeQueue<int, 4> q;
  int v[4] = {0, 1, 2, 3};
  EXPECT_TRUE(q.push(&v[0]));
  EXPECT_TRUE(q.push(&v[1]));
  EXPECT_TRUE(q.push(&v[2]));
  EXPECT_TRUE(q.full());
  // Ring reserves one slot: the SIZEth push fails and is counted as dropped.
  EXPECT_FALSE(q.push(&v[3]));
  EXPECT_EQ(q.get_and_reset_dropped_count(), 1u);
  EXPECT_EQ(q.get_and_reset_dropped_count(), 0u);  // reset is sticky
}

TEST(LockFreeQueueNoAtomics, NullPushRejected) {
  esphome::LockFreeQueue<int, 4> q;
  EXPECT_FALSE(q.push(nullptr));
  EXPECT_TRUE(q.empty());
}

TEST(LockFreeQueueNoAtomics, WrapAround) {
  esphome::LockFreeQueue<int, 4> q;
  int v[3] = {10, 20, 30};
  // Cycle several times the ring size to cross the wrap boundary repeatedly.
  for (int cycle = 0; cycle < 10; cycle++) {
    for (auto &value : v)
      ASSERT_TRUE(q.push(&value));
    EXPECT_TRUE(q.full());
    for (auto &value : v)
      ASSERT_EQ(q.pop(), &value);
    EXPECT_TRUE(q.empty());
  }
  EXPECT_EQ(q.get_and_reset_dropped_count(), 0u);
}

TEST(LockFreeQueueNoAtomics, IncrementDroppedCount) {
  esphome::LockFreeQueue<int, 4> q;
  // Producer-side external drop accounting (pool exhausted before push).
  q.increment_dropped_count();
  q.increment_dropped_count();
  EXPECT_EQ(q.get_and_reset_dropped_count(), 2u);
}

TEST(LockFreeQueueNoAtomics, InterleavedPushPop) {
  esphome::LockFreeQueue<int, 8> q;
  int v[64];
  int popped = 0;
  for (int i = 0; i < 64; i++) {
    v[i] = i;
    ASSERT_TRUE(q.push(&v[i]));
    if (i % 2 == 1) {
      int *first = q.pop();
      ASSERT_NE(first, nullptr);
      EXPECT_EQ(*first, popped++);
      int *second = q.pop();
      ASSERT_NE(second, nullptr);
      EXPECT_EQ(*second, popped++);
    }
  }
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(popped, 64);
}

}  // namespace esphome::core::testing
