#include "esphome/core/event_pool.h"

#include <gtest/gtest.h>

#include <set>

namespace esphome::core::testing {

struct PoolItem {
  int value{0};
  // EventPool contract: release() cleans up per-object state; nothing here.
  void release() {}
};

TEST(EventPool, AllocateUpToCapacityThenNull) {
  esphome::EventPool<PoolItem, 4> pool;
  PoolItem *items[4];
  for (auto *&item : items) {
    item = pool.allocate();
    ASSERT_NE(item, nullptr);
  }
  // At capacity: the pool refuses rather than growing past SIZE.
  EXPECT_EQ(pool.allocate(), nullptr);
}

TEST(EventPool, FullDrainRetainsEveryObject) {
  // Pins the SIZE + 1 free-list sizing: a fully returned pool must hold all
  // SIZE objects. With a SIZE-slot ring (capacity SIZE - 1) the last release()
  // of a full drain was dropped, permanently orphaning one object.
  esphome::EventPool<PoolItem, 4> pool;
  PoolItem *items[4];
  for (auto *&item : items)
    item = pool.allocate();
  for (auto *item : items)
    pool.release(item);

  // Every object must be allocatable again — no orphan, no new creation
  // (total_created_ is already at SIZE, so a lost object would surface as a
  // nullptr on the fourth allocation).
  std::set<PoolItem *> seen;
  for (int i = 0; i < 4; i++) {
    PoolItem *item = pool.allocate();
    ASSERT_NE(item, nullptr);
    seen.insert(item);
  }
  // And they are the same four objects, recycled rather than re-created.
  for (auto *item : items)
    EXPECT_TRUE(seen.count(item) == 1);
  EXPECT_EQ(pool.allocate(), nullptr);
}

TEST(EventPool, RepeatedDrainCyclesAreStable) {
  esphome::EventPool<PoolItem, 3> pool;
  // Several full allocate/release cycles: capacity must not shrink over time.
  for (int cycle = 0; cycle < 10; cycle++) {
    PoolItem *items[3];
    for (auto *&item : items) {
      item = pool.allocate();
      ASSERT_NE(item, nullptr);
    }
    EXPECT_EQ(pool.allocate(), nullptr);
    for (auto *item : items)
      pool.release(item);
  }
}

TEST(EventPool, ReleaseNullptrIsSafe) {
  esphome::EventPool<PoolItem, 2> pool;
  pool.release(nullptr);
  EXPECT_NE(pool.allocate(), nullptr);
}

TEST(EventPool, WarmFullyPopulatesThePool) {
  // warm()'s guarantee is invisible at runtime: no later allocate() may touch
  // malloc(). Fully populated means SIZE allocations succeed from the free
  // list and the SIZE + 1-th refuses.
  esphome::EventPool<PoolItem, 4> pool;
  ASSERT_TRUE(pool.warm());
  PoolItem *items[4];
  for (auto *&item : items) {
    item = pool.allocate();
    ASSERT_NE(item, nullptr);
  }
  EXPECT_EQ(pool.allocate(), nullptr);
}

TEST(EventPool, WarmIsIdempotent) {
  esphome::EventPool<PoolItem, 3> pool;
  ASSERT_TRUE(pool.warm());
  ASSERT_TRUE(pool.warm());
  // Still exactly SIZE objects: no growth past capacity.
  PoolItem *items[3];
  for (auto *&item : items) {
    item = pool.allocate();
    ASSERT_NE(item, nullptr);
  }
  EXPECT_EQ(pool.allocate(), nullptr);
}

TEST(EventPool, AllocateAfterWarmRecyclesTheWarmedObjects) {
  // The objects handed out after warm() are the ones warm() created,
  // recycled rather than re-created.
  esphome::EventPool<PoolItem, 4> pool;
  ASSERT_TRUE(pool.warm());
  std::set<PoolItem *> first_round;
  PoolItem *items[4];
  for (auto *&item : items) {
    item = pool.allocate();
    first_round.insert(item);
  }
  for (auto *item : items)
    pool.release(item);
  for (int i = 0; i < 4; i++) {
    PoolItem *item = pool.allocate();
    ASSERT_NE(item, nullptr);
    EXPECT_TRUE(first_round.count(item) == 1);
  }
}

TEST(EventPool, WarmTopsUpWithEntriesOutstanding) {
  // warm() counts existing entries (free or checked out) instead of failing
  // when some are outstanding: it tops the pool up from any state.
  esphome::EventPool<PoolItem, 4> pool;
  PoolItem *held = pool.allocate();
  ASSERT_NE(held, nullptr);
  ASSERT_TRUE(pool.warm());
  // The held object plus three more accounts for all SIZE entries.
  PoolItem *items[3];
  for (auto *&item : items) {
    item = pool.allocate();
    ASSERT_NE(item, nullptr);
  }
  EXPECT_EQ(pool.allocate(), nullptr);
}

}  // namespace esphome::core::testing
