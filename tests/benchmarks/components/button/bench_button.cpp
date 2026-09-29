#include <benchmark/benchmark.h>

#include "esphome/components/button/button.h"

namespace esphome::button::benchmarks {

static constexpr int kInnerIterations = 2000;

// Minimal Button for benchmarking — press_action() is a no-op.
class BenchButton : public Button {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

 protected:
  void press_action() override {}
};

// --- Button::press() ---
// Measures: ESP_LOGD + press_action() + callback dispatch.

static void ButtonPress(benchmark::State &state) {
  BenchButton button;
  button.configure("test_button");

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      button.press();
    }
    benchmark::DoNotOptimize(&button);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ButtonPress);

// --- Button::press() with callback ---
// Measures callback dispatch overhead.

static void ButtonPress_WithCallback(benchmark::State &state) {
  BenchButton button;
  button.configure("test_button");

  uint64_t callback_count = 0;
  button.add_on_press_callback([&callback_count]() { callback_count++; });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      button.press();
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ButtonPress_WithCallback);

}  // namespace esphome::button::benchmarks
