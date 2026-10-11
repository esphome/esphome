// Exercises the LockFreeQueue PlainAtomic path under ESPHOME_THREAD_SINGLE —
// the gate added for single-threaded platforms whose only concurrency is
// same-core interrupt preemption (RP2: BTstack packet handler in the CYW43
// async-context IRQ). The define is forced before the include so this TU
// deterministically compiles that path regardless of the host's default
// thread model. The instantiations here deliberately differ from
// test_lock_free_queue.cpp's (uint32_t elements, non-power-of-2 sizes): no
// template instantiation is shared between the two TUs, so the differing
// AtomicIndex definitions can never collide under the one-definition rule,
// and the non-power-of-2 sizes cover next_index()'s comparison branch, which
// the other TU's power-of-2 sizes never reach.
#define ESPHOME_THREAD_SINGLE
#include "esphome/core/lock_free_queue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <type_traits>

namespace esphome::core::testing {

// Pin the gate itself: under ESPHOME_THREAD_SINGLE the index type must be the
// PlainAtomic fallback, not std::atomic — otherwise the RP2040 build silently
// pulls __atomic_* library calls back in.
static_assert(!std::is_same_v<esphome::lockfree_internal::AtomicIndex<uint8_t>, std::atomic<uint8_t>>,
              "ESPHOME_THREAD_SINGLE must select the PlainAtomic index path");

TEST(LockFreeQueueThreadSingle, EmptyPopReturnsNull) {
  esphome::LockFreeQueue<uint32_t, 5> q;
  EXPECT_EQ(q.pop(), nullptr);
  EXPECT_TRUE(q.empty());
  EXPECT_FALSE(q.full());
  EXPECT_EQ(q.size(), 0u);
}

TEST(LockFreeQueueThreadSingle, FifoOrder) {
  esphome::LockFreeQueue<uint32_t, 5> q;
  uint32_t a = 1, b = 2, c = 3, d = 4;
  EXPECT_TRUE(q.push(&a));
  EXPECT_TRUE(q.push(&b));
  EXPECT_TRUE(q.push(&c));
  EXPECT_TRUE(q.push(&d));
  EXPECT_EQ(q.size(), 4u);
  EXPECT_EQ(q.pop(), &a);
  EXPECT_EQ(q.pop(), &b);
  EXPECT_EQ(q.pop(), &c);
  EXPECT_EQ(q.pop(), &d);
  EXPECT_EQ(q.pop(), nullptr);
}

TEST(LockFreeQueueThreadSingle, CapacityIsSizeMinusOne) {
  esphome::LockFreeQueue<uint32_t, 5> q;
  uint32_t v[5] = {0, 1, 2, 3, 4};
  EXPECT_TRUE(q.push(&v[0]));
  EXPECT_TRUE(q.push(&v[1]));
  EXPECT_TRUE(q.push(&v[2]));
  EXPECT_TRUE(q.push(&v[3]));
  EXPECT_TRUE(q.full());
  // Ring reserves one slot: the SIZEth push fails and is counted as dropped.
  EXPECT_FALSE(q.push(&v[4]));
  EXPECT_EQ(q.get_and_reset_dropped_count(), 1u);
  EXPECT_EQ(q.get_and_reset_dropped_count(), 0u);  // reset is sticky
}

TEST(LockFreeQueueThreadSingle, NullPushRejected) {
  esphome::LockFreeQueue<uint32_t, 5> q;
  EXPECT_FALSE(q.push(nullptr));
  EXPECT_TRUE(q.empty());
}

TEST(LockFreeQueueThreadSingle, WrapAround) {
  // Non-power-of-2 SIZE: next_index() wraps via the comparison branch here.
  esphome::LockFreeQueue<uint32_t, 5> q;
  uint32_t v[4] = {10, 20, 30, 40};
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

TEST(LockFreeQueueThreadSingle, IncrementDroppedCount) {
  esphome::LockFreeQueue<uint32_t, 5> q;
  // Producer-side external drop accounting (pool exhausted before push).
  q.increment_dropped_count();
  q.increment_dropped_count();
  EXPECT_EQ(q.get_and_reset_dropped_count(), 2u);
}

TEST(LockFreeQueueThreadSingle, InterleavedPushPop) {
  esphome::LockFreeQueue<uint32_t, 7> q;
  uint32_t v[64];
  uint32_t popped = 0;
  for (uint32_t i = 0; i < 64; i++) {
    v[i] = i;
    ASSERT_TRUE(q.push(&v[i]));
    if (i % 2 == 1) {
      uint32_t *first = q.pop();
      ASSERT_NE(first, nullptr);
      EXPECT_EQ(*first, popped++);
      uint32_t *second = q.pop();
      ASSERT_NE(second, nullptr);
      EXPECT_EQ(*second, popped++);
    }
  }
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(popped, 64u);
}

}  // namespace esphome::core::testing
