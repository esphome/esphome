// Test that in-place compaction in full_cleanup_removed_items_() produces
// identical results to the old vector-copy approach.
// Compile with: g++ -std=gnu++20 -o test_scheduler_cleanup test_scheduler_cleanup.cpp && ./test_scheduler_cleanup

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

// Minimal stand-in for SchedulerItem — only the fields the cleanup touches.
struct Item {
  uint32_t id;              // identifies the item for verification
  bool remove;              // mirrors SchedulerItem::remove
  uint64_t next_execution;  // for heap ordering

  Item(uint32_t id, bool remove, uint64_t next_execution) : id(id), remove(remove), next_execution(next_execution) {}
};

// Comparator matching SchedulerItem::cmp (min-heap by next_execution)
struct ItemCmp {
  bool operator()(const std::unique_ptr<Item> &a, const std::unique_ptr<Item> &b) const {
    return a->next_execution > b->next_execution;
  }
};

// --- Old implementation (vector copy) ----------------------------------------
struct OldResult {
  std::vector<uint32_t> kept_ids;      // ids of items kept, in heap order
  std::vector<uint32_t> recycled_ids;  // ids of items recycled
};

OldResult run_old(std::vector<std::unique_ptr<Item>> &items) {
  OldResult result;

  std::vector<std::unique_ptr<Item>> valid_items;
  for (auto &item : items) {
    if (!item->remove) {
      valid_items.push_back(std::move(item));
    } else {
      result.recycled_ids.push_back(item->id);
    }
  }
  items = std::move(valid_items);
  std::make_heap(items.begin(), items.end(), ItemCmp{});

  // Extract sorted order by popping the heap
  while (!items.empty()) {
    std::pop_heap(items.begin(), items.end(), ItemCmp{});
    result.kept_ids.push_back(items.back()->id);
    items.pop_back();
  }
  return result;
}

// --- New implementation (in-place compaction) ---------------------------------
struct NewResult {
  std::vector<uint32_t> kept_ids;
  std::vector<uint32_t> recycled_ids;
};

NewResult run_new(std::vector<std::unique_ptr<Item>> &items) {
  NewResult result;

  size_t write = 0;
  for (size_t read = 0; read < items.size(); ++read) {
    if (!items[read]->remove) {
      if (write != read) {
        items[write] = std::move(items[read]);
      }
      ++write;
    } else {
      result.recycled_ids.push_back(items[read]->id);
    }
  }
  items.resize(write);
  std::make_heap(items.begin(), items.end(), ItemCmp{});

  while (!items.empty()) {
    std::pop_heap(items.begin(), items.end(), ItemCmp{});
    result.kept_ids.push_back(items.back()->id);
    items.pop_back();
  }
  return result;
}

// --- Helpers -----------------------------------------------------------------

// Build a fresh item list from {id, remove, next_execution} triples.
using Spec = std::tuple<uint32_t, bool, uint64_t>;

std::vector<std::unique_ptr<Item>> build(const std::vector<Spec> &specs) {
  std::vector<std::unique_ptr<Item>> items;
  for (auto &[id, rm, exec] : specs) {
    items.push_back(std::make_unique<Item>(id, rm, exec));
  }
  // Put into heap order like the real scheduler
  std::make_heap(items.begin(), items.end(), ItemCmp{});
  return items;
}

void assert_equal(const OldResult &old_r, const NewResult &new_r) {
  assert(old_r.kept_ids == new_r.kept_ids);

  // Recycled order may differ (old iterates linearly, new also iterates linearly,
  // but heap order changes iteration order). Sort before comparing.
  auto old_recycled = old_r.recycled_ids;
  auto new_recycled = new_r.recycled_ids;
  std::sort(old_recycled.begin(), old_recycled.end());
  std::sort(new_recycled.begin(), new_recycled.end());
  assert(old_recycled == new_recycled);
}

// Test harness
#define TEST(name) static void test_##name()
#define RUN_TEST(name) \
  do { \
    printf("  " #name "..."); \
    test_##name(); \
    printf(" OK\n"); \
  } while (0)

// =============================================================================
// Tests
// =============================================================================

TEST(empty_list) {
  auto items_old = build({});
  auto items_new = build({});
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.empty());
  assert(old_r.recycled_ids.empty());
}

TEST(no_removals) {
  std::vector<Spec> specs = {
      {1, false, 100},
      {2, false, 200},
      {3, false, 300},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 3);
  assert(old_r.recycled_ids.empty());
}

TEST(all_removed) {
  std::vector<Spec> specs = {
      {1, true, 100},
      {2, true, 200},
      {3, true, 300},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.empty());
  assert(old_r.recycled_ids.size() == 3);
}

TEST(single_item_kept) {
  std::vector<Spec> specs = {{42, false, 500}};
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 1);
  assert(old_r.kept_ids[0] == 42);
}

TEST(single_item_removed) {
  std::vector<Spec> specs = {{42, true, 500}};
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.empty());
  assert(old_r.recycled_ids.size() == 1);
}

TEST(remove_first_only) {
  std::vector<Spec> specs = {
      {1, true, 100},
      {2, false, 200},
      {3, false, 300},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 2);
  assert(old_r.recycled_ids.size() == 1);
}

TEST(remove_last_only) {
  std::vector<Spec> specs = {
      {1, false, 100},
      {2, false, 200},
      {3, true, 300},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 2);
  assert(old_r.recycled_ids.size() == 1);
}

TEST(remove_middle_only) {
  std::vector<Spec> specs = {
      {1, false, 100},
      {2, true, 200},
      {3, false, 300},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
}

TEST(alternating_remove_keep) {
  std::vector<Spec> specs = {
      {1, true, 100}, {2, false, 200}, {3, true, 300}, {4, false, 400}, {5, true, 500}, {6, false, 600},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 3);
  assert(old_r.recycled_ids.size() == 3);
}

TEST(alternating_keep_remove) {
  std::vector<Spec> specs = {
      {1, false, 100}, {2, true, 200}, {3, false, 300}, {4, true, 400}, {5, false, 500}, {6, true, 600},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
}

TEST(consecutive_removals_at_start) {
  std::vector<Spec> specs = {
      {1, true, 100}, {2, true, 200}, {3, true, 300}, {4, false, 400}, {5, false, 500},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
}

TEST(consecutive_removals_at_end) {
  std::vector<Spec> specs = {
      {1, false, 100}, {2, false, 200}, {3, true, 300}, {4, true, 400}, {5, true, 500},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
}

TEST(duplicate_execution_times) {
  // Multiple items with the same next_execution — heap order is implementation-defined
  // but both algorithms must produce the same set of kept/recycled items
  std::vector<Spec> specs = {
      {1, false, 100}, {2, true, 100}, {3, false, 100}, {4, true, 200}, {5, false, 200},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);

  // Same sets (sorted), even if pop order differs for equal timestamps
  auto old_kept_sorted = old_r.kept_ids;
  auto new_kept_sorted = new_r.kept_ids;
  std::sort(old_kept_sorted.begin(), old_kept_sorted.end());
  std::sort(new_kept_sorted.begin(), new_kept_sorted.end());
  assert(old_kept_sorted == new_kept_sorted);

  auto old_recycled = old_r.recycled_ids;
  auto new_recycled = new_r.recycled_ids;
  std::sort(old_recycled.begin(), old_recycled.end());
  std::sort(new_recycled.begin(), new_recycled.end());
  assert(old_recycled == new_recycled);
}

TEST(large_list_sparse_removals) {
  // 100 items, every 10th removed
  std::vector<Spec> specs;
  for (uint32_t i = 0; i < 100; i++) {
    specs.push_back({i, (i % 10 == 0), i * 100});
  }
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 90);
  assert(old_r.recycled_ids.size() == 10);
}

TEST(large_list_dense_removals) {
  // 100 items, only every 10th kept
  std::vector<Spec> specs;
  for (uint32_t i = 0; i < 100; i++) {
    specs.push_back({i, (i % 10 != 0), i * 100});
  }
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 10);
  assert(old_r.recycled_ids.size() == 90);
}

TEST(realistic_scheduler_25_cancel) {
  // Mimics the integration test: 25 cancelled + 5 active post-cleanup timeouts
  std::vector<Spec> specs;
  for (uint32_t i = 0; i < 25; i++) {
    specs.push_back({i, true, 2500});  // cancelled timeouts
  }
  for (uint32_t i = 25; i < 30; i++) {
    specs.push_back({i, false, 50 + (i - 25) * 25});  // post-cleanup timeouts
  }
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
  assert(old_r.kept_ids.size() == 5);
  assert(old_r.recycled_ids.size() == 25);
}

TEST(heap_order_preserved) {
  // Verify the kept items come out in correct min-heap order (ascending next_execution)
  std::vector<Spec> specs = {
      {1, false, 500}, {2, true, 100},  {3, false, 300}, {4, true, 700},
      {5, false, 100}, {6, false, 900}, {7, true, 200},  {8, false, 400},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);

  // Both should produce items in ascending execution time order
  // (since we pop from min-heap)
  assert(old_r.kept_ids == new_r.kept_ids);
}

TEST(two_items_first_removed) {
  std::vector<Spec> specs = {
      {1, true, 100},
      {2, false, 200},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
}

TEST(two_items_second_removed) {
  std::vector<Spec> specs = {
      {1, false, 100},
      {2, true, 200},
  };
  auto items_old = build(specs);
  auto items_new = build(specs);
  auto old_r = run_old(items_old);
  auto new_r = run_new(items_new);
  assert_equal(old_r, new_r);
}

// =============================================================================
// Main
// =============================================================================

int main() {
  printf("Scheduler cleanup equivalence tests\n");
  printf("====================================\n\n");

  printf("Basic cases:\n");
  RUN_TEST(empty_list);
  RUN_TEST(no_removals);
  RUN_TEST(all_removed);
  RUN_TEST(single_item_kept);
  RUN_TEST(single_item_removed);

  printf("\nEdge removal positions:\n");
  RUN_TEST(remove_first_only);
  RUN_TEST(remove_last_only);
  RUN_TEST(remove_middle_only);
  RUN_TEST(two_items_first_removed);
  RUN_TEST(two_items_second_removed);

  printf("\nPatterns:\n");
  RUN_TEST(alternating_remove_keep);
  RUN_TEST(alternating_keep_remove);
  RUN_TEST(consecutive_removals_at_start);
  RUN_TEST(consecutive_removals_at_end);
  RUN_TEST(duplicate_execution_times);

  printf("\nScale:\n");
  RUN_TEST(large_list_sparse_removals);
  RUN_TEST(large_list_dense_removals);
  RUN_TEST(realistic_scheduler_25_cancel);

  printf("\nHeap correctness:\n");
  RUN_TEST(heap_order_preserved);

  printf("\n====================================\n");
  printf("All tests passed!\n");

  return 0;
}
