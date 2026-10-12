#include <benchmark/benchmark.h>

#include "esphome/components/number/number.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Minimal Number for benchmarking — control() publishes the value back.
class BenchNumber : public number::Number {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

 protected:
  void control(float value) override { this->publish_state(value); }
};

// Helper to create a typical number entity for benchmarks.
static void setup_number(BenchNumber &number) {
  number.configure("test_number");
  number.traits.set_min_value(0.0f);
  number.traits.set_max_value(100.0f);
  number.traits.set_step(1.0f);
  number.traits.set_mode(number::NUMBER_MODE_SLIDER);
}

// --- Number::publish_state() ---
// Measures the publish path: set_has_state, store value, callback dispatch.

static void NumberPublish_State(benchmark::State &state) {
  BenchNumber number;
  setup_number(number);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      number.publish_state(static_cast<float>(i % 100));
    }
    benchmark::DoNotOptimize(number.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(NumberPublish_State);

// --- Number::publish_state() with callback ---
// Measures callback dispatch overhead.

static void NumberPublish_WithCallback(benchmark::State &state) {
  BenchNumber number;
  setup_number(number);

  uint64_t callback_count = 0;
  number.add_on_state_callback([&callback_count](float) { callback_count++; });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      number.publish_state(static_cast<float>(i % 100));
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(NumberPublish_WithCallback);

// --- NumberCall::perform() set value ---
// The most common number call — setting an absolute value.
// Exercises: validation against min/max, control() dispatch.

static void NumberCall_SetValue(benchmark::State &state) {
  BenchNumber number;
  setup_number(number);
  number.publish_state(50.0f);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      float val = static_cast<float>(i % 100);
      number.make_call().set_value(val).perform();
    }
    benchmark::DoNotOptimize(number.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(NumberCall_SetValue);

// --- NumberCall::perform() increment ---
// Exercises: state read, step arithmetic, max clamping.

static void NumberCall_Increment(benchmark::State &state) {
  BenchNumber number;
  setup_number(number);
  number.publish_state(0.0f);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      number.make_call().number_increment(true).perform();
    }
    benchmark::DoNotOptimize(number.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(NumberCall_Increment);

// --- NumberCall::perform() decrement ---
// Exercises: state read, step arithmetic, min clamping.

static void NumberCall_Decrement(benchmark::State &state) {
  BenchNumber number;
  setup_number(number);
  number.publish_state(100.0f);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      number.make_call().number_decrement(true).perform();
    }
    benchmark::DoNotOptimize(number.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(NumberCall_Decrement);

}  // namespace esphome::benchmarks
