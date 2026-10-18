#include <benchmark/benchmark.h>

#include "esphome/components/select/select.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Minimal Select for benchmarking — control() publishes directly by index.
class BenchSelect : public select::Select {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

 protected:
  void control(size_t index) override { this->publish_state(index); }
};

// Helper to create a select with the given options.
static void setup_select(BenchSelect &select, const char *name, std::initializer_list<const char *> options) {
  select.configure(name);
  select.traits.set_options(options);
  select.publish_state(size_t(0));
}

// --- Select::publish_state(size_t) ---
// The fast path: publish by index, no string lookup.

static void SelectPublish_ByIndex(benchmark::State &state) {
  BenchSelect select;
  setup_select(select, "test_select", {"off", "still", "move", "still+move"});

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      select.publish_state(static_cast<size_t>(i % 4));
    }
    benchmark::DoNotOptimize(select.active_index());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SelectPublish_ByIndex);

// --- Select::publish_state(const char *) ---
// The string path: requires index_of() lookup via strncmp.

static void SelectPublish_ByString(benchmark::State &state) {
  BenchSelect select;
  setup_select(select, "test_select", {"off", "still", "move", "still+move"});

  const char *options[] = {"off", "still", "move", "still+move"};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      select.publish_state(options[i % 4]);
    }
    benchmark::DoNotOptimize(select.active_index());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SelectPublish_ByString);

// --- Select::publish_state() with callback ---
// Measures callback dispatch overhead on the index path.

static void SelectPublish_WithCallback(benchmark::State &state) {
  BenchSelect select;
  setup_select(select, "test_select", {"off", "still", "move", "still+move"});

  uint64_t callback_count = 0;
  select.add_on_state_callback([&callback_count](size_t) { callback_count++; });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      select.publish_state(static_cast<size_t>(i % 4));
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SelectPublish_WithCallback);

// --- SelectCall::perform() set by index ---
// The fast call path — no string matching needed.

static void SelectCall_SetByIndex(benchmark::State &state) {
  BenchSelect select;
  setup_select(select, "test_select", {"off", "still", "move", "still+move"});

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      select.make_call().set_index(i % 4).perform();
    }
    benchmark::DoNotOptimize(select.active_index());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SelectCall_SetByIndex);

// --- SelectCall::perform() set by option string ---
// Exercises the string lookup path through index_of().

static void SelectCall_SetByOption(benchmark::State &state) {
  BenchSelect select;
  setup_select(select, "test_select", {"off", "still", "move", "still+move"});

  const char *options[] = {"off", "still", "move", "still+move"};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      select.make_call().set_option(options[i % 4]).perform();
    }
    benchmark::DoNotOptimize(select.active_index());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SelectCall_SetByOption);

// --- SelectCall::perform() next with cycling ---
// Exercises the navigation path through active_index_.

static void SelectCall_NextCycle(benchmark::State &state) {
  BenchSelect select;
  setup_select(select, "test_select", {"off", "still", "move", "still+move"});

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      select.make_call().select_next(true).perform();
    }
    benchmark::DoNotOptimize(select.active_index());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SelectCall_NextCycle);

// --- SelectCall with 10 options (string lookup) ---
// Worst-case string matching with more options.

static void SelectCall_SetByOption_10Options(benchmark::State &state) {
  BenchSelect select;
  setup_select(
      select, "test_select",
      {"off", "still", "move", "still+move", "custom1", "custom2", "custom3", "custom4", "custom5", "custom6"});

  // Pick options spread across the list to exercise different search depths
  const char *picks[] = {"off", "custom3", "custom6", "move"};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      select.make_call().set_option(picks[i % 4]).perform();
    }
    benchmark::DoNotOptimize(select.active_index());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SelectCall_SetByOption_10Options);

}  // namespace esphome::benchmarks
