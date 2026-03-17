#include <benchmark/benchmark.h>

#include "esphome/core/application.h"

namespace esphome::benchmarks {

// Benchmark Application::loop() with no registered components.
// App is initialized by original_setup() in main.cpp (code-generated
// pre_setup, area/device registration, looping_components_.init).
// This measures the baseline overhead of the main loop: scheduler,
// timing, before/after loop tasks, and yield_with_select_.
static void ApplicationLoop_Empty(benchmark::State &state) {
  for (auto _ : state) {
    App.loop();
  }
}
BENCHMARK(ApplicationLoop_Empty);

}  // namespace esphome::benchmarks
