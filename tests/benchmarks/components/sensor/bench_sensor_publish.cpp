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

// --- Sensor::publish_state() with alternating values ---
// Forces has_state transition on first call and state change every iteration.
// Tests the full path including has_state flag management.

static void SensorPublish_StateChange(benchmark::State &state) {
  TestSensor sensor;
  sensor.configure("test_sensor");

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      // Alternate between two values to force state change every time
      float value = (i & 1) ? 23.5f : 42.0f;
      sensor.publish_state(value);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SensorPublish_StateChange);

}  // namespace esphome::benchmarks
