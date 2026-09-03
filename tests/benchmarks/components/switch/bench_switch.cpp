#include <benchmark/benchmark.h>

#include "esphome/components/switch/switch.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Minimal Switch for benchmarking — write_state() publishes directly.
class BenchSwitch : public switch_::Switch {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

 protected:
  void write_state(bool state) override { this->publish_state(state); }
};

// --- Switch::publish_state() alternating ---
// Forces state change every call, exercising the full publish path.

static void SwitchPublish_Alternating(benchmark::State &state) {
  BenchSwitch sw;
  sw.configure("test_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.publish_state(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sw.publish_state(i % 2 == 0);
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SwitchPublish_Alternating);

// --- Switch::publish_state() no change ---
// Tests the deduplication fast path in publish_dedup_.

static void SwitchPublish_NoChange(benchmark::State &state) {
  BenchSwitch sw;
  sw.configure("test_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.publish_state(true);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sw.publish_state(true);
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SwitchPublish_NoChange);

// --- Switch::publish_state() with callback ---
// Measures callback dispatch overhead on state changes.

static void SwitchPublish_WithCallback(benchmark::State &state) {
  BenchSwitch sw;
  sw.configure("test_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);

  uint64_t callback_count = 0;
  sw.add_on_state_callback([&callback_count](bool) { callback_count++; });
  sw.publish_state(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sw.publish_state(i % 2 == 0);
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SwitchPublish_WithCallback);

// --- Switch::turn_on() / turn_off() ---
// The front-end call path: turn_on → write_state → publish_state.

static void SwitchTurnOn(benchmark::State &state) {
  BenchSwitch sw;
  sw.configure("test_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.publish_state(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sw.turn_on();
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SwitchTurnOn);

// --- Switch::toggle() alternating ---
// Exercises the toggle path which reads current state to determine target.

static void SwitchToggle(benchmark::State &state) {
  BenchSwitch sw;
  sw.configure("test_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.publish_state(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sw.toggle();
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SwitchToggle);

// --- Switch::publish_state() inverted ---
// Verifies the inversion path doesn't add significant overhead.

static void SwitchPublish_Inverted(benchmark::State &state) {
  BenchSwitch sw;
  sw.configure("test_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.set_inverted(true);
  sw.publish_state(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sw.publish_state(i % 2 == 0);
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(SwitchPublish_Inverted);

}  // namespace esphome::benchmarks
