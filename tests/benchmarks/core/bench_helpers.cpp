#include <benchmark/benchmark.h>

#include "esphome/core/helpers.h"

namespace esphome::benchmarks {

// --- random_float() ---
// Ported from ol.yaml:148 "Random Float Benchmark"

static void RandomFloat(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(random_float());
  }
}
BENCHMARK(RandomFloat);

// --- random_uint32() ---

static void RandomUint32(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(random_uint32());
  }
}
BENCHMARK(RandomUint32);

}  // namespace esphome::benchmarks
