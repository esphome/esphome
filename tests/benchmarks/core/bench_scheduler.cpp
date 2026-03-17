#include <benchmark/benchmark.h>

#include "esphome/core/scheduler.h"
#include "esphome/core/hal.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 1000;

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

// --- Scheduler: next_schedule_in() calculation ---

static void Scheduler_NextScheduleIn(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

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
