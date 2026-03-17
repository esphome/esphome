#include <benchmark/benchmark.h>

#include "esphome/core/helpers.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// --- random_float() ---
// Ported from ol.yaml:148 "Random Float Benchmark"

static void RandomFloat(benchmark::State &state) {
  for (auto _ : state) {
    float result = 0.0f;
    for (int i = 0; i < kInnerIterations; i++) {
      result += random_float();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(RandomFloat);

// --- random_uint32() ---

static void RandomUint32(benchmark::State &state) {
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += random_uint32();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(RandomUint32);

}  // namespace esphome::benchmarks
