#include <benchmark/benchmark.h>

#include "esphome/core/log.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

static const char *const TAG = "bench";

// --- Log a message with no format specifiers (fastest path) ---

static void Logger_NoFormat(benchmark::State &state) {
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ESP_LOGW(TAG, "Something happened");
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Logger_NoFormat);

// --- Log a message with 3 uint32_t format specifiers ---

static void Logger_3Uint32(benchmark::State &state) {
  uint32_t a = 12345, b = 67890, c = 99999;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ESP_LOGW(TAG, "Values: %" PRIu32 " %" PRIu32 " %" PRIu32, a, b, c);
    }
    benchmark::DoNotOptimize(a);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Logger_3Uint32);

// --- Log a message with 3 floats (common for sensor values) ---

static void Logger_3Float(benchmark::State &state) {
  float temp = 23.456f, humidity = 67.89f, pressure = 1013.25f;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ESP_LOGW(TAG, "Sensor: %.2f %.1f %.2f", temp, humidity, pressure);
    }
    benchmark::DoNotOptimize(temp);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Logger_3Float);

}  // namespace esphome::benchmarks
