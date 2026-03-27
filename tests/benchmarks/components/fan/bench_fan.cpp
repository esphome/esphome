#include <benchmark/benchmark.h>

#include "esphome/components/fan/fan.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Minimal Fan for benchmarking — control() is a no-op.
class BenchFan : public fan::Fan {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

  fan::FanTraits get_traits() override { return this->traits_; }

  fan::FanTraits traits_;

 protected:
  void control(const fan::FanCall & /*call*/) override {}
};

// Helper to create a typical fan device for benchmarks.
// Note: setup() is not called (no preferences backend), so save_state_()
// is effectively a no-op. This benchmarks the call/validation path, not persistence.
static void setup_fan(BenchFan &fan) {
  fan.configure("test_fan");
  fan.traits_.set_oscillation(true);
  fan.traits_.set_speed(true);
  fan.traits_.set_supported_speed_count(6);
  fan.traits_.set_direction(true);
  fan.set_restore_mode(fan::FanRestoreMode::NO_RESTORE);
  fan.traits_.set_supported_preset_modes({
      "auto",
      "sleep",
      "nature",
      "turbo",
  });
}

// --- Fan::publish_state() with speed update ---
// Measures the publish path for a fan reporting state —
// the hot path during fan operation.

static void FanPublish_State(benchmark::State &state) {
  BenchFan fan;
  setup_fan(fan);
  fan.state = true;
  fan.direction = fan::FanDirection::FORWARD;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      fan.speed = (i % 6) + 1;
      fan.publish_state();
    }
    benchmark::DoNotOptimize(fan.speed);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FanPublish_State);

// --- Fan::publish_state() with callback ---
// Measures callback dispatch overhead.

static void FanPublish_WithCallback(benchmark::State &state) {
  BenchFan fan;
  setup_fan(fan);
  fan.state = true;

  uint64_t callback_count = 0;
  fan.add_on_state_callback([&callback_count]() { callback_count++; });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      fan.speed = (i % 6) + 1;
      fan.publish_state();
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FanPublish_WithCallback);

// --- FanCall::perform() set speed ---
// The most common fan call — adjusting the speed level.

static void FanCall_SetSpeed(benchmark::State &state) {
  BenchFan fan;
  setup_fan(fan);
  fan.state = true;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      int speed = (i % 6) + 1;
      fan.make_call().set_speed(speed).perform();
    }
    benchmark::DoNotOptimize(fan.speed);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FanCall_SetSpeed);

// --- FanCall::perform() with multiple fields ---
// Exercises the validation path with state, speed, oscillation, and direction.

static void FanCall_MultiField(benchmark::State &state) {
  BenchFan fan;
  setup_fan(fan);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      auto dir = (i % 2 == 0) ? fan::FanDirection::FORWARD : fan::FanDirection::REVERSE;
      int speed = (i % 6) + 1;
      fan.make_call().set_state(true).set_speed(speed).set_oscillating(i % 2 == 0).set_direction(dir).perform();
    }
    benchmark::DoNotOptimize(fan.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FanCall_MultiField);

}  // namespace esphome::benchmarks
