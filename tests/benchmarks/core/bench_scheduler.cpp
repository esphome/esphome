#include <benchmark/benchmark.h>

#include "esphome/core/scheduler.h"
#include "esphome/core/hal.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
// Must be divisible by all batch sizes used below (3, 10) to avoid
// pool imbalance at iteration boundaries that causes spurious malloc.
static constexpr int kInnerIterations = 2100;

// Warm the scheduler pool by registering and replacing items twice.
// The first batch allocates fresh items; the second batch cancels them and
// populates the recycling pool with the cancelled items from the first batch.
static void warm_pool(Scheduler &scheduler, Component *component, int batch_size, uint32_t delay) {
  uint32_t now = millis();
  for (int i = 0; i < batch_size; i++) {
    scheduler.set_timeout(component, static_cast<uint32_t>(i), delay, []() {});
  }
  scheduler.call(++now);
  for (int i = 0; i < batch_size; i++) {
    scheduler.set_timeout(component, static_cast<uint32_t>(i), delay, []() {});
  }
  scheduler.call(++now);
}

// --- Scheduler fast path: no work to do ---

static void Scheduler_Call_NoWork(benchmark::State &state) {
  Scheduler scheduler;
  uint32_t now = millis();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.call(now);
    }
    benchmark::DoNotOptimize(now);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Call_NoWork);

// --- Scheduler with timers: call() when timers exist but aren't due ---

static void Scheduler_Call_TimersNotDue(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // Add some timeouts far in the future
  for (int i = 0; i < 10; i++) {
    scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(i), 1000000, []() {});
  }
  scheduler.process_to_add();

  uint32_t now = millis();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.call(now);
    }
    benchmark::DoNotOptimize(now);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Call_TimersNotDue);

// --- Scheduler with 5 intervals firing every call ---

static void Scheduler_Call_5IntervalsFiring(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;
  int fire_count = 0;

  // Benchmarks the heap-based scheduler dispatch with 5 callbacks firing.
  // Uses monotonically increasing fake time so intervals reliably fire every call.
  // USE_BENCHMARK ifdef in component.h disables WarnIfComponentBlockingGuard
  // (fake now > real millis() would cause underflow in finish()).
  // interval=0 would cause an infinite loop (reschedules at same now).
  for (int i = 0; i < 5; i++) {
    scheduler.set_interval(&dummy_component, static_cast<uint32_t>(i), 1, [&fire_count]() { fire_count++; });
  }
  scheduler.process_to_add();

  uint32_t now = millis() + 100;

  for (auto _ : state) {
    scheduler.call(now);
    now++;
    benchmark::DoNotOptimize(fire_count);
  }
}
BENCHMARK(Scheduler_Call_5IntervalsFiring);

// --- Scheduler: set_timeout registration ---

static void Scheduler_SetTimeout(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // Register 3 timeouts then call() — realistic worst case where multiple
  // components schedule in the same loop iteration. warm_pool fills the
  // freelist so acquire/recycle never falls back to malloc.
  static constexpr int kBatchSize = 3;
  static_assert(kInnerIterations % kBatchSize == 0, "kInnerIterations must be divisible by kBatchSize");
  warm_pool(scheduler, &dummy_component, kBatchSize, 1000);
  for (auto _ : state) {
    uint32_t now = millis();
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(i % kBatchSize), 1000, []() {});
      if ((i + 1) % kBatchSize == 0) {
        scheduler.call(++now);
      }
    }
    scheduler.call(++now);
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_SetTimeout);

// --- Scheduler: set_interval registration ---

static void Scheduler_SetInterval(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // Register 3 intervals then call() — realistic worst case where multiple
  // components schedule in the same loop iteration. Keeps item count within
  // the recycling pool (MAX_POOL_SIZE=5) to avoid spurious malloc/free.
  static constexpr int kBatchSize = 3;
  static_assert(kInnerIterations % kBatchSize == 0, "kInnerIterations must be divisible by kBatchSize");
  warm_pool(scheduler, &dummy_component, kBatchSize, 1000);
  for (auto _ : state) {
    uint32_t now = millis();
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_interval(&dummy_component, static_cast<uint32_t>(i % kBatchSize), 1000, []() {});
      if ((i + 1) % kBatchSize == 0) {
        scheduler.call(++now);
      }
    }
    scheduler.call(++now);
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_SetInterval);

// --- Scheduler: defer registration (set_timeout with delay=0) ---

static void Scheduler_Defer(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // defer() is Component::defer which calls set_timeout(delay=0).
  // Component::defer(func) passes nullptr as the name, which skips
  // cancel_item_locked_ entirely — matching production behavior where
  // defers are anonymous fire-and-forget callbacks.
  static constexpr int kBatchSize = 3;
  static_assert(kInnerIterations % kBatchSize == 0, "kInnerIterations must be divisible by kBatchSize");
  warm_pool(scheduler, &dummy_component, kBatchSize, 0);
  for (auto _ : state) {
    uint32_t now = millis();
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_timeout(&dummy_component, static_cast<const char *>(nullptr), 0, []() {});
      if ((i + 1) % kBatchSize == 0) {
        scheduler.call(++now);
      }
    }
    scheduler.call(++now);
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Defer);

// --- Scheduler: defer with same ID (cancel-and-replace pattern) ---

static void Scheduler_Defer_SameID(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // Measures defer with a fixed numeric ID — each call cancels the previous
  // pending defer before adding the new one. This is the pattern used by
  // components that defer work but want to coalesce rapid updates.
  static constexpr int kBatchSize = 3;
  static_assert(kInnerIterations % kBatchSize == 0, "kInnerIterations must be divisible by kBatchSize");
  warm_pool(scheduler, &dummy_component, kBatchSize, 0);
  for (auto _ : state) {
    uint32_t now = millis();
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(0), 0, []() {});
      if ((i + 1) % kBatchSize == 0) {
        scheduler.call(++now);
      }
    }
    scheduler.call(++now);
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Defer_SameID);

// --- Scheduler: set_timeout with batch size exceeding pool (cliff test) ---

static void Scheduler_SetTimeout_ExceedPool(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // Register 10 timeouts then call() — larger working set than the 3-item
  // batches above. With the unbounded freelist, warm_pool preallocates 10
  // items so this measures steady-state, not malloc cliff.
  static constexpr int kBatchSize = 10;
  static_assert(kInnerIterations % kBatchSize == 0, "kInnerIterations must be divisible by kBatchSize");
  warm_pool(scheduler, &dummy_component, kBatchSize, 1000);
  for (auto _ : state) {
    uint32_t now = millis();
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(i % kBatchSize), 1000, []() {});
      if ((i + 1) % kBatchSize == 0) {
        scheduler.call(++now);
      }
    }
    scheduler.call(++now);
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_SetTimeout_ExceedPool);

}  // namespace esphome::benchmarks
