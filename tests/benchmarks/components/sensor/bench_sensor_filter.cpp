#include <benchmark/benchmark.h>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensor/filter.h"
#include "esphome/core/application.h"

#include <limits>

namespace esphome::sensor::benchmarks {

static constexpr int kInnerIterations = 2000;
// Long enough that interval filters never fire during a run
static constexpr uint32_t kNeverFireMs = 3600000;

static constexpr float NO_MAX_DELTA = std::numeric_limits<float>::infinity();
static constexpr uint32_t LOOP_TICK_MS = 16;

static inline void advance_ramp(float &value) {
  value += 0.1f;
  if (value > 1000.0f)
    value = 0.0f;
}

// Publish a 0..1000 ramp in 0.1 steps
static void publish_ramp(benchmark::State &state, Sensor &sensor) {
  float value = 0.0f;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(value);
      advance_ramp(value);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}

// Publish the ramp one loop tick apart from t=1, then restore the cached loop time for later benchmarks
static void publish_ramp_ticked(benchmark::State &state, Sensor &sensor) {
  const uint32_t saved = App.get_loop_component_start_time();
  uint32_t now = 1;
  float value = 0.0f;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      LoopBlockingGuard tick{nullptr, nullptr, now};
      sensor.publish_state(value);
      now += LOOP_TICK_MS;
      advance_ramp(value);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
  LoopBlockingGuard restore{nullptr, nullptr, saved};
}

// Benchmark: sensor publish through a SlidingWindowMovingAverageFilter (window=5, send_every=1)
static void SensorFilter_SlidingWindowAvg(benchmark::State &state) {
  Sensor sensor;

  // Create filter: window_size=5, send_every=1, send_first_at=1
  auto *filter = new SlidingWindowMovingAverageFilter(5, 1, 1);
  sensor.add_filter(filter);

  publish_ramp(state, sensor);
}
BENCHMARK(SensorFilter_SlidingWindowAvg);

// Benchmark: sensor publish through ExponentialMovingAverageFilter
static void SensorFilter_ExponentialMovingAvg(benchmark::State &state) {
  Sensor sensor;

  // alpha=0.1, send_every=1, send_first_at=1
  auto *filter = new ExponentialMovingAverageFilter(0.1f, 1, 1);
  sensor.add_filter(filter);

  publish_ramp(state, sensor);
}
BENCHMARK(SensorFilter_ExponentialMovingAvg);

// Benchmark: sensor publish through a chain of 3 filters (offset + multiply + sliding window)
static void SensorFilter_Chain3(benchmark::State &state) {
  Sensor sensor;

  sensor.add_filters({
      new OffsetFilter([]() -> float { return 1.0f; }),
      new MultiplyFilter([]() -> float { return 2.0f; }),
      new SlidingWindowMovingAverageFilter(5, 1, 1),
  });

  publish_ramp(state, sensor);
}
BENCHMARK(SensorFilter_Chain3);

// Benchmark: throttle 1s, one publish per 16ms loop tick (1 in 63 passes)
static void SensorFilter_Throttle(benchmark::State &state) {
  Sensor sensor;
  sensor.add_filter(new ThrottleFilter(1000));
  publish_ramp_ticked(state, sensor);
}
BENCHMARK(SensorFilter_Throttle);

// Benchmark: delta 0.5 on a 0.1 ramp (1 in 5 passes)
static void SensorFilter_Delta(benchmark::State &state) {
  Sensor sensor;
  sensor.add_filter(new DeltaFilter(0.5f, 0.0f, NO_MAX_DELTA, 0.0f));
  publish_ramp(state, sensor);
}
BENCHMARK(SensorFilter_Delta);

// Fixed zig-zag so the median window is rarely already sorted
static constexpr float MEDIAN_INPUT[16] = {3.0f, 9.0f, 1.0f, 7.0f, 4.0f, 8.0f, 2.0f, 6.0f,
                                           5.0f, 0.0f, 9.5f, 1.5f, 7.5f, 3.5f, 8.5f, 2.5f};

// Benchmark: median of 5, computed on every publish
static void SensorFilter_Median(benchmark::State &state) {
  Sensor sensor;
  sensor.add_filter(new MedianFilter(5, 1, 1));
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++)
      sensor.publish_state(MEDIAN_INPUT[i % 16]);
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorFilter_Median);

// Benchmark: throttle_average accumulate path (interval never fires)
static void SensorFilter_ThrottleAverage(benchmark::State &state) {
  Sensor sensor;
  auto *filter = new ThrottleAverageFilter(kNeverFireMs);
  sensor.add_filter(filter);
  publish_ramp(state, sensor);
  App.scheduler.cancel_interval(filter);
  App.scheduler.process_to_add();
}
BENCHMARK(SensorFilter_ThrottleAverage);

// Benchmark: heartbeat store path (interval never fires)
static void SensorFilter_Heartbeat(benchmark::State &state) {
  Sensor sensor;
  auto *filter = new HeartbeatFilter(kNeverFireMs);
  sensor.add_filter(filter);
  publish_ramp(state, sensor);
  App.scheduler.cancel_interval(filter);
  App.scheduler.process_to_add();
}
BENCHMARK(SensorFilter_Heartbeat);

// Same shape as the function codegen emits for a lambda filter
static optional<float> lambda_filter_fn(float x) {
  if (x < 0.0f)
    return {};
  return x * 1.8f + 32.0f;
}

// Benchmark: lambda filter as generated (ApplyFilter)
static void SensorFilter_Lambda(benchmark::State &state) {
  Sensor sensor;
  sensor.add_filter(new ApplyFilter<lambda_filter_fn>());
  publish_ramp(state, sensor);
}
BENCHMARK(SensorFilter_Lambda);

// Benchmark: round to 1 decimal
static void SensorFilter_Round(benchmark::State &state) {
  Sensor sensor;
  sensor.add_filter(new RoundFilter(1));
  publish_ramp(state, sensor);
}
BENCHMARK(SensorFilter_Round);

// Benchmark: or of throttle 60s and delta 0.5, the common "report on change or every minute", one publish per tick
static void SensorFilter_Or(benchmark::State &state) {
  Sensor sensor;
  sensor.add_filter(new OrFilter<2>(new ThrottleFilter(60000), new DeltaFilter(0.5f, 0.0f, NO_MAX_DELTA, 0.0f)));
  publish_ramp_ticked(state, sensor);
}
BENCHMARK(SensorFilter_Or);

}  // namespace esphome::sensor::benchmarks
