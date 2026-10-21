#include <benchmark/benchmark.h>

#include "esphome/components/climate/climate.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Minimal Climate for benchmarking — control() is a no-op.
class BenchClimate : public climate::Climate {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

  climate::ClimateTraits traits() override { return this->traits_; }

  climate::ClimateTraits traits_;

 protected:
  void control(const climate::ClimateCall & /*call*/) override {}
};

// Helper to create a typical HVAC climate device for benchmarks.
// Note: setup() is not called (no preferences backend), so save_state_()
// is effectively a no-op. This benchmarks the call/validation path, not persistence.
static void setup_hvac_climate(BenchClimate &climate) {
  climate.configure("test_climate");
  climate.traits_.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT_COOL,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_FAN_ONLY,
  });
  climate.traits_.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });
  climate.traits_.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_BOTH,
      climate::CLIMATE_SWING_VERTICAL,
      climate::CLIMATE_SWING_HORIZONTAL,
  });
  climate.traits_.set_supported_presets({
      climate::CLIMATE_PRESET_NONE,
      climate::CLIMATE_PRESET_HOME,
      climate::CLIMATE_PRESET_AWAY,
  });
  climate.traits_.set_visual_min_temperature(16.0f);
  climate.traits_.set_visual_max_temperature(30.0f);
  climate.traits_.set_visual_target_temperature_step(0.5f);
  climate.traits_.set_visual_current_temperature_step(0.1f);
  climate.traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
}

// --- Climate::publish_state() with temperature update ---
// Measures the publish path for a thermostat reporting state —
// the hot path during HVAC operation.

static void ClimatePublish_State(benchmark::State &state) {
  BenchClimate climate;
  setup_hvac_climate(climate);
  climate.mode = climate::CLIMATE_MODE_HEAT;
  climate.action = climate::CLIMATE_ACTION_HEATING;
  climate.target_temperature = 22.0f;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      climate.current_temperature = 20.0f + static_cast<float>(i % 100) / 10.0f;
      climate.publish_state();
    }
    benchmark::DoNotOptimize(climate.current_temperature);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ClimatePublish_State);

// --- Climate::publish_state() with callback ---
// Measures callback dispatch overhead.

static void ClimatePublish_WithCallback(benchmark::State &state) {
  BenchClimate climate;
  setup_hvac_climate(climate);
  climate.mode = climate::CLIMATE_MODE_HEAT;
  climate.target_temperature = 22.0f;

  uint64_t callback_count = 0;
  climate.add_on_state_callback([&callback_count](climate::Climate & /*c*/) { callback_count++; });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      climate.current_temperature = 20.0f + static_cast<float>(i % 100) / 10.0f;
      climate.publish_state();
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ClimatePublish_WithCallback);

// --- ClimateCall::perform() set target temperature ---
// The most common climate call — adjusting the thermostat setpoint.

static void ClimateCall_SetTemperature(benchmark::State &state) {
  BenchClimate climate;
  setup_hvac_climate(climate);
  climate.mode = climate::CLIMATE_MODE_HEAT;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      float temp = 18.0f + static_cast<float>(i % 25) * 0.5f;
      climate.make_call().set_target_temperature(temp).perform();
    }
    benchmark::DoNotOptimize(climate.target_temperature);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ClimateCall_SetTemperature);

// --- ClimateCall::perform() mode change with fan ---
// Exercises the validation path with multiple fields set.

static void ClimateCall_ModeChange(benchmark::State &state) {
  BenchClimate climate;
  setup_hvac_climate(climate);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      auto mode = (i % 2 == 0) ? climate::CLIMATE_MODE_HEAT : climate::CLIMATE_MODE_COOL;
      auto fan = (i % 2 == 0) ? climate::CLIMATE_FAN_HIGH : climate::CLIMATE_FAN_LOW;
      climate.make_call().set_mode(mode).set_fan_mode(fan).set_target_temperature(22.0f).perform();
    }
    benchmark::DoNotOptimize(climate.mode);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ClimateCall_ModeChange);

}  // namespace esphome::benchmarks
