#include <benchmark/benchmark.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"
#include "esphome/core/application.h"

namespace esphome::binary_sensor::benchmarks {

static constexpr int kInnerIterations = 2000;
// Long enough that no delay fires during a run, so every publish is a bounce
static constexpr uint32_t DELAY_MS = 3600000;

// Alternate the input with one scheduler pass per publish, like a bouncing GPIO polled each loop
static void publish_bounce(benchmark::State &state, BinarySensor &sensor, bool run_scheduler = true) {
  sensor.publish_initial_state(false);
  const uint32_t now = millis();
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(i % 2 == 0);
      if (run_scheduler)
        App.scheduler.call(now);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
  // Resetting the filters cancels their pending timers before the filter leaves scope
  sensor.invalidate_state();
  App.scheduler.call(now);
}

// Benchmark: no filter, one scheduler pass per publish; subtract it from the delayed filters below
static void BinarySensorFilter_SchedulerBaseline(benchmark::State &state) {
  BinarySensor sensor;
  publish_bounce(state, sensor);
}
BENCHMARK(BinarySensorFilter_SchedulerBaseline);

// Benchmark: invert, no scheduler pass since it schedules nothing, so not comparable to the five below
static void BinarySensorFilter_Invert(benchmark::State &state) {
  BinarySensor sensor;
  InvertFilter filter;
  sensor.add_filter(&filter);
  publish_bounce(state, sensor, false);
}
BENCHMARK(BinarySensorFilter_Invert);

// Benchmark: delayed_on bounce, set_timeout on press and cancel on release
static void BinarySensorFilter_DelayedOn(benchmark::State &state) {
  BinarySensor sensor;
  DelayedOnFilter filter;
  filter.set_delay([]() -> uint32_t { return DELAY_MS; });
  sensor.add_filter(&filter);
  publish_bounce(state, sensor);
}
BENCHMARK(BinarySensorFilter_DelayedOn);

// Benchmark: delayed_off bounce, cancel on press and set_timeout on release
static void BinarySensorFilter_DelayedOff(benchmark::State &state) {
  BinarySensor sensor;
  DelayedOffFilter filter;
  filter.set_delay([]() -> uint32_t { return DELAY_MS; });
  sensor.add_filter(&filter);
  publish_bounce(state, sensor);
}
BENCHMARK(BinarySensorFilter_DelayedOff);

// Same shape as the function codegen emits for delayed_on_off with time_on and time_off
static optional<bool> delayed_on_off_fn(Filter *filter, bool x) {
  return delay_output(filter, x ? DELAY_MS : DELAY_MS / 2, x);
}

// Benchmark: delayed_on_off bounce, every edge replaces the pending timeout
static void BinarySensorFilter_DelayedOnOff(benchmark::State &state) {
  BinarySensor sensor;
  ApplyFilter<delayed_on_off_fn> filter;
  sensor.add_filter(&filter);
  publish_bounce(state, sensor);
}
BENCHMARK(BinarySensorFilter_DelayedOnOff);

// Benchmark: settle bounce, the first edge passes and the rest re-arm the timeout
static void BinarySensorFilter_Settle(benchmark::State &state) {
  BinarySensor sensor;
  SettleFilter filter;
  filter.set_delay([]() -> uint32_t { return DELAY_MS; });
  sensor.add_filter(&filter);
  publish_bounce(state, sensor);
}
BENCHMARK(BinarySensorFilter_Settle);

// Benchmark: autorepeat press and release, arming the first step then cancelling both timers
static void BinarySensorFilter_Autorepeat(benchmark::State &state) {
  BinarySensor sensor;
  AutorepeatFilter<1> filter({{DELAY_MS, 100, 900}});
  sensor.add_filter(&filter);
  publish_bounce(state, sensor);
}
BENCHMARK(BinarySensorFilter_Autorepeat);

}  // namespace esphome::binary_sensor::benchmarks
