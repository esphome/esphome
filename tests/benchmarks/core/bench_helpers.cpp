#include <benchmark/benchmark.h>

#include "esphome/core/helpers.h"

namespace esphome::benchmarks {

// --- random_float() ---
// Ported from ol.yaml:148 "Random Float Benchmark"

static void BM_RandomFloat(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(random_float());
  }
}
BENCHMARK(BM_RandomFloat);

// --- random_uint32() ---

static void BM_RandomUint32(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(random_uint32());
  }
}
BENCHMARK(BM_RandomUint32);

}  // namespace esphome::benchmarks
