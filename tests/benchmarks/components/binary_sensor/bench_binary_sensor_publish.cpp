#include <benchmark/benchmark.h>

#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::binary_sensor::benchmarks {

static constexpr int kInnerIterations = 2000;

// Benchmark: publish_state with alternating values (forces state change every time)
static void BinarySensorPublish_Alternating(benchmark::State &state) {
  BinarySensor sensor;

  // First publish to establish initial state
  sensor.publish_initial_state(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(i % 2 == 0);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(BinarySensorPublish_Alternating);

// Benchmark: publish_state with same value (tests dedup fast path)
static void BinarySensorPublish_NoChange(benchmark::State &state) {
  BinarySensor sensor;

  sensor.publish_initial_state(true);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(true);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(BinarySensorPublish_NoChange);

// Benchmark: publish_state with a callback registered
static void BinarySensorPublish_WithCallback(benchmark::State &state) {
  BinarySensor sensor;

  int callback_count = 0;
  sensor.add_on_state_callback([&callback_count](bool) { callback_count++; });

  sensor.publish_initial_state(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(i % 2 == 0);
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(BinarySensorPublish_WithCallback);

}  // namespace esphome::binary_sensor::benchmarks
