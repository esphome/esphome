#include <benchmark/benchmark.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/hal.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Optimistic switch: write_state() publishes directly, like an optimistic template switch.
class AutomationBenchSwitch : public switch_::Switch {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

 protected:
  void write_state(bool state) override { this->publish_state(state); }
};

class AutomationBenchSensor : public sensor::Sensor {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345679, 0); }
};

// Entities the generated-style functions below act on, like the codegen globals.
static AutomationBenchSwitch *bench_switch = nullptr;
static binary_sensor::BinarySensor *bench_binary_sensor = nullptr;

// Generated action and condition functions have this shape.
static void switch_toggle_fn(const float &x) { bench_switch->toggle(); }
static void switch_turn_on_fn(const float &x) { bench_switch->turn_on(); }
static void switch_turn_off_fn(const float &x) { bench_switch->turn_off(); }
static bool binary_sensor_is_on_fn(const float &x) { return bench_binary_sensor->state; }

static void setup_switch(AutomationBenchSwitch &sw) {
  sw.configure("bench_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.publish_state(false);
  bench_switch = &sw;
}

// --- Automation with one action ---
// TriggerForwarder -> Automation -> ActionList -> ApplyAction (switch.toggle).

static void Automation_SingleAction(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);

  Automation<float> automation;
  ApplyAction<switch_toggle_fn, float> toggle;
  automation.add_action(&toggle);
  TriggerForwarder<float> forwarder{&automation};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      forwarder(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_SingleAction);

// --- Automation with three chained actions ---
// Actions that cannot merge into one function each walk the ActionList chain.

static void Automation_MultipleActions(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);

  Automation<float> automation;
  ApplyAction<switch_turn_on_fn, float> turn_on;
  ApplyAction<switch_turn_off_fn, float> turn_off;
  ApplyAction<switch_toggle_fn, float> toggle;
  automation.add_action(&turn_on);
  automation.add_action(&turn_off);
  automation.add_action(&toggle);
  TriggerForwarder<float> forwarder{&automation};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      forwarder(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_MultipleActions);

// --- if/then/else with a lambda condition ---
// Alternating values take both branches, so the switch changes state every time.

static void Automation_IfLambdaCondition(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);

  StatelessLambdaCondition<float> condition([](float x) -> bool { return x > 10; });
  IfAction<true, float> if_action(&condition);
  ApplyAction<switch_turn_on_fn, float> turn_on;
  ApplyAction<switch_turn_off_fn, float> turn_off;
  if_action.add_then_action(&turn_on);
  if_action.finish_then();
  if_action.add_else_action(&turn_off);
  if_action.finish_else();
  Automation<float> automation;
  automation.add_action(&if_action);
  TriggerForwarder<float> forwarder{&automation};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      forwarder(i % 2 == 0 ? 20.0f : 0.0f);
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_IfLambdaCondition);

// --- if/then with a binary_sensor.is_on condition ---
// ApplyCondition reads the entity state; the then branch toggles the switch.

static void Automation_IfStateCondition(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  binary_sensor::BinarySensor binary_sensor;
  binary_sensor.publish_initial_state(true);
  bench_binary_sensor = &binary_sensor;

  ApplyCondition<binary_sensor_is_on_fn, float> condition;
  IfAction<false, float> if_action(&condition);
  ApplyAction<switch_toggle_fn, float> toggle;
  if_action.add_then_action(&toggle);
  if_action.finish_then();
  Automation<float> automation;
  automation.add_action(&if_action);
  TriggerForwarder<float> forwarder{&automation};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      forwarder(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_IfStateCondition);

// --- delay action scheduling ---
// Each trigger schedules the delay on App.scheduler; stop() cancels it so runs never pile up.

static void Automation_DelaySchedule(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);

  DelayAction<float> delay;
  delay.set_delay([](float x) -> uint32_t { return 1000; });
  ApplyAction<switch_toggle_fn, float> toggle;
  Automation<float> automation;
  automation.add_action(&delay);
  automation.add_action(&toggle);
  TriggerForwarder<float> forwarder{&automation};

  // Batch size matches the scheduler benchmarks so the item pool stays warm.
  static constexpr int kBatchSize = 4;
  static_assert(kInnerIterations % kBatchSize == 0, "kInnerIterations must be divisible by kBatchSize");
  uint32_t now = millis();
  for (int i = 0; i < kBatchSize; i++) {
    forwarder(0.0f);
    automation.stop();
  }
  App.scheduler.call(++now);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      forwarder(static_cast<float>(i));
      automation.stop();
      if ((i + 1) % kBatchSize == 0) {
        App.scheduler.call(++now);
      }
    }
    benchmark::DoNotOptimize(automation);
  }
  App.scheduler.call(++now);
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_DelaySchedule);

// --- sensor on_value with inline actions ---
// publish_state() runs the capture-less callback codegen emits, which calls the merged action function.

static void Automation_SensorOnValue(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  AutomationBenchSensor sensor;
  sensor.configure("bench_sensor");
  sensor.add_on_state_callback([](const float &x) -> void { switch_toggle_fn(x); });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_SensorOnValue);

// --- binary_sensor on_press with inline actions ---
// Alternating states; the generated callback returns early on release.

static void Automation_BinarySensorOnPress(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  binary_sensor::BinarySensor binary_sensor;
  binary_sensor.publish_initial_state(false);
  binary_sensor.add_on_state_callback([](const bool &pressed) -> void {
    if (!pressed)
      return;
    switch_toggle_fn(0.0f);
  });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      binary_sensor.publish_state(i % 2 == 0);
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_BinarySensorOnPress);

}  // namespace esphome::benchmarks
