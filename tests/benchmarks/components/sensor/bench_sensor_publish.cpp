#include <benchmark/benchmark.h>

#include "esphome/components/sensor/sensor.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// Test subclass to access protected configure_entity_() for benchmark setup.
class TestSensor : public sensor::Sensor {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }
};

// --- Sensor::publish_state() with no callbacks registered ---
// Measures baseline publish overhead: state assignment, logging,
// internal_send_state_to_frontend, ControllerRegistry notification.

static void SensorPublish_NoCallbacks(benchmark::State &state) {
  TestSensor sensor;
  sensor.configure("test_sensor");

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorPublish_NoCallbacks);

// --- Sensor::publish_state() with one state callback ---
// Measures callback dispatch overhead through LazyCallbackManager.

static void SensorPublish_WithCallback(benchmark::State &state) {
  TestSensor sensor;
  sensor.configure("test_sensor");

  float callback_value = 0.0f;
  sensor.add_on_state_callback([&callback_value](float value) { callback_value = value; });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(callback_value);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorPublish_WithCallback);

// --- Sensor::publish_state() with the same value every time ---
// Steady-state pattern: sensor reports an unchanged reading.
// Sensor doesn't dedup today, so this exercises the same code path
// as changing values, but tracks the common real-world pattern
// separately for regression detection.

static void SensorPublish_SameValue(benchmark::State &state) {
  TestSensor sensor;
  sensor.configure("test_sensor");

  // Warm up so has_state is already set
  sensor.publish_state(23.5f);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(23.5f);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorPublish_SameValue);

}  // namespace esphome::benchmarks
