#include <benchmark/benchmark.h>

#include "esphome/core/scheduler.h"
#include "esphome/core/hal.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// Component subclass that suppresses blocking warnings.
// Under valgrind, 2000 inner iterations take long enough in wall clock
// to trigger WarnIfComponentBlockingGuard. Setting the threshold to max
// prevents log noise without affecting the benchmarked code path.
class BenchComponent : public Component {
 public:
  BenchComponent() { this->warn_if_blocking_over_ = UINT16_MAX; }
};

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
  BenchComponent dummy_component;

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
  BenchComponent dummy_component;
  int fire_count = 0;

  // Add 5 intervals with 1ms period — they fire every call when time advances.
  // We use monotonically increasing fake time (now++) so intervals reliably fire.
  // WARN_IF_BLOCKING_OVER_MS=UINT32_MAX in benchmark build flags prevents the
  // WarnIfComponentBlockingGuard from triggering when fake time exceeds real millis().
  // Note: interval=0 causes an infinite loop (reschedules at same time, never breaks).
  for (int i = 0; i < 5; i++) {
    scheduler.set_interval(&dummy_component, static_cast<uint32_t>(i), 1, [&fire_count]() { fire_count++; });
  }
  scheduler.process_to_add();

  uint32_t now = millis() + 100;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.call(now);
      now++;
    }
    benchmark::DoNotOptimize(fire_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Call_5IntervalsFiring);

// --- Scheduler: next_schedule_in() calculation ---

static void Scheduler_NextScheduleIn(benchmark::State &state) {
  Scheduler scheduler;
  BenchComponent dummy_component;

  // Add some timeouts
  for (int i = 0; i < 10; i++) {
    scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(i), 1000 * (i + 1), []() {});
  }
  scheduler.process_to_add();

  uint32_t now = millis();

  for (auto _ : state) {
    optional<uint32_t> result;
    for (int i = 0; i < kInnerIterations; i++) {
      result = scheduler.next_schedule_in(now);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_NextScheduleIn);

}  // namespace esphome::benchmarks
