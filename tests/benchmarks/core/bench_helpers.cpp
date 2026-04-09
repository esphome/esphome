#include <benchmark/benchmark.h>
#include <cinttypes>
#include <cstdio>

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

// --- uint32_to_str() vs snprintf ---

static void Uint32ToStr_Small(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uint32_to_str(buf, 12345);
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Uint32ToStr_Small);

static void Snprintf_Uint32_Small(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      snprintf(buf, sizeof(buf), "%" PRIu32, static_cast<uint32_t>(12345));
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Snprintf_Uint32_Small);

static void Uint32ToStr_Large(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uint32_to_str(buf, 4294967295u);
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Uint32ToStr_Large);

static void Snprintf_Uint32_Large(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      snprintf(buf, sizeof(buf), "%" PRIu32, static_cast<uint32_t>(4294967295u));
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Snprintf_Uint32_Large);

}  // namespace esphome::benchmarks
