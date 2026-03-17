#include <benchmark/benchmark.h>

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::benchmarks {

// A minimal component with an empty loop for benchmarking dispatch overhead
class NoopComponent : public Component {
 public:
  void loop() override {}
  float get_setup_priority() const override { return 0.0f; }
  // Expose protected method for benchmarking
  void set_loop_state() { this->set_component_state_(COMPONENT_STATE_LOOP); }
};

// --- WarnIfComponentBlockingGuard overhead ---

static void BM_WarnIfComponentBlockingGuard(benchmark::State &state) {
  NoopComponent component;
  uint32_t now = millis();

  for (auto _ : state) {
    WarnIfComponentBlockingGuard guard{&component, now};
    now = guard.finish();
    benchmark::DoNotOptimize(now);
  }
}
BENCHMARK(BM_WarnIfComponentBlockingGuard);

// --- Component virtual dispatch overhead ---

static void BM_ComponentDispatch(benchmark::State &state) {
  NoopComponent component;
  component.set_loop_state();
  uint32_t now = millis();

  for (auto _ : state) {
    {
      WarnIfComponentBlockingGuard guard{&component, now};
      component.loop();
      now = guard.finish();
    }
    benchmark::DoNotOptimize(now);
  }
}
BENCHMARK(BM_ComponentDispatch);

}  // namespace esphome::benchmarks
