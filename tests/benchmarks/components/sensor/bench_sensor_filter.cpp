#include <benchmark/benchmark.h>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensor/filter.h"

namespace esphome::sensor::benchmarks {

static constexpr int kInnerIterations = 2000;

// Benchmark: sensor publish through a SlidingWindowMovingAverageFilter (window=5, send_every=1)
static void SensorFilter_SlidingWindowAvg(benchmark::State &state) {
  Sensor sensor;

  // Create filter: window_size=5, send_every=1, send_first_at=1
  auto *filter = new SlidingWindowMovingAverageFilter(5, 1, 1);
  sensor.add_filter(filter);

  float value = 0.0f;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(value);
      value += 0.1f;
      if (value > 1000.0f)
        value = 0.0f;
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorFilter_SlidingWindowAvg);

// Benchmark: sensor publish through ExponentialMovingAverageFilter
static void SensorFilter_ExponentialMovingAvg(benchmark::State &state) {
  Sensor sensor;

  // alpha=0.1, send_every=1, send_first_at=1
  auto *filter = new ExponentialMovingAverageFilter(0.1f, 1, 1);
  sensor.add_filter(filter);

  float value = 0.0f;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(value);
      value += 0.1f;
      if (value > 1000.0f)
        value = 0.0f;
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorFilter_ExponentialMovingAvg);

// Benchmark: sensor publish through a chain of 3 filters (offset + multiply + sliding window)
static void SensorFilter_Chain3(benchmark::State &state) {
  Sensor sensor;

  sensor.add_filters({
      new OffsetFilter(1.0f),
      new MultiplyFilter(2.0f),
      new SlidingWindowMovingAverageFilter(5, 1, 1),
  });

  float value = 0.0f;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(value);
      value += 0.1f;
      if (value > 1000.0f)
        value = 0.0f;
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorFilter_Chain3);

}  // namespace esphome::sensor::benchmarks
