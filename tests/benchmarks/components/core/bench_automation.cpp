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

static AutomationBenchSwitch *bench_switch = nullptr;
static binary_sensor::BinarySensor *bench_binary_sensor = nullptr;
static uint32_t bench_counter = 0;

static void setup_switch(AutomationBenchSwitch &sw) {
  sw.configure("bench_switch");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.publish_state(false);
  bench_switch = &sw;
}

static void setup_binary_sensor(binary_sensor::BinarySensor &binary_sensor, bool state) {
  binary_sensor.publish_initial_state(state);
  bench_binary_sensor = &binary_sensor;
}

// The functions below copy the shapes codegen emits in main.cpp.

static void toggle_fn(const float &x) { bench_switch->toggle(); }

static void toggle_no_args_fn() { bench_switch->toggle(); }

static void counter_fn(const float &x) { bench_counter++; }

static void merged_toggles_fn(const int &esphome__running, const float &x) {
  {
    bench_switch->toggle();
  }
  if (!esphome__running)
    return;
  {
    bench_switch->toggle();
  }
  if (!esphome__running)
    return;
  {
    bench_switch->toggle();
  }
}

static void if_lambda_fn(const float &x) {
  {
    {
      const bool esphome__condition = ([](float x) -> bool { return x > 10; }(x));
      if (esphome__condition) {
        {
          bench_switch->turn_on();
        }
      } else {
        {
          bench_switch->turn_off();
        }
      }
    }
  }
}

static void if_state_fn(const float &x) {
  {
    {
      const bool esphome__condition = (bench_binary_sensor->state);
      if (esphome__condition) {
        {
          bench_switch->toggle();
        }
      }
    }
  }
}

static void setup_sensor(AutomationBenchSensor &sensor) { sensor.configure("bench_sensor"); }

// --- on_value: [lambda: 'id(counter)++;'] ---
// Dispatch only: the direct function bumps a counter, with no switch publishing.

static void Automation_DispatchOnly(benchmark::State &state) {
  AutomationBenchSensor sensor;
  setup_sensor(sensor);
  sensor.add_on_state_callback([](const float &x) -> void { counter_fn(x); });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(bench_counter);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_DispatchOnly);

// --- on_value: [switch.toggle] ---
// One inline action: the callback calls one direct function, no Automation object.

static void Automation_SensorOnValue(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  AutomationBenchSensor sensor;
  setup_sensor(sensor);
  sensor.add_on_state_callback([](const float &x) -> void { toggle_fn(x); });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_SensorOnValue);

// --- on_value with automation_id: [switch.toggle x3] ---
// The toggles merge into one ApplyRunAction in an Automation, the chain [toggle x3, delay: 1s] also
// builds; the delay is left out here, Automation_DelaySchedule covers it.

static void Automation_MergedRun(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  AutomationBenchSensor sensor;
  setup_sensor(sensor);
  Automation<float> automation;
  ApplyRunAction<merged_toggles_fn, float> run;
  automation.add_action(&run);
  sensor.add_on_state_callback(TriggerForwarder<float>{&automation});

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_MergedRun);

// --- on_value: [if: {condition: lambda, then: [switch.turn_on], else: [switch.turn_off]}] ---
// Inline branches become statements in one direct function; alternating values flip the switch.

static void Automation_IfInline(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  AutomationBenchSensor sensor;
  setup_sensor(sensor);
  sensor.add_on_state_callback([](const float &x) -> void { if_lambda_fn(x); });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(i % 2 == 0 ? 20.0f : 0.0f);
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_IfInline);

// --- on_value: [if: {condition: binary_sensor.is_on, then: [switch.toggle]}] ---
// The condition reads the entity state inline in the direct function.

static void Automation_IfStateInline(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  binary_sensor::BinarySensor binary_sensor;
  setup_binary_sensor(binary_sensor, true);
  AutomationBenchSensor sensor;
  setup_sensor(sensor);
  sensor.add_on_state_callback([](const float &x) -> void { if_state_fn(x); });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_IfStateInline);

// --- on_value: [if: {condition: lambda, then: [delay: 1s, switch.toggle], else: [switch.toggle]}] ---
// The delay forces the IfAction object path; values stay low so only the else branch runs.

static void Automation_IfActionObject(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  AutomationBenchSensor sensor;
  setup_sensor(sensor);
  DelayAction<float> delay;
  delay.set_delay([](float x) -> uint32_t { return 1000; });
  StatelessLambdaCondition<float> condition([](float x) -> bool { return x > 10; });
  IfAction<true, float> if_action(&condition);
  ApplyAction<toggle_fn, float> then_toggle;
  ApplyAction<toggle_fn, float> else_toggle;
  if_action.add_then_action(&delay);
  if_action.add_then_action(&then_toggle);
  if_action.finish_then();
  if_action.add_else_action(&else_toggle);
  if_action.finish_else();
  Automation<float> automation;
  automation.add_action(&if_action);
  sensor.add_on_state_callback(TriggerForwarder<float>{&automation});

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i % 10));
    }
    benchmark::DoNotOptimize(sw.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_IfActionObject);

// --- on_value: [delay: 1s, ...] ---
// Each trigger schedules the delay; stop() cancels it and the scheduler runs once per trigger.

static void Automation_DelaySchedule(benchmark::State &state) {
  AutomationBenchSensor sensor;
  setup_sensor(sensor);
  DelayAction<float> delay;
  delay.set_delay([](float x) -> uint32_t { return 1000; });
  Automation<float> automation;
  automation.add_action(&delay);
  sensor.add_on_state_callback(TriggerForwarder<float>{&automation});

  uint32_t now = millis();
  sensor.publish_state(0.0f);
  automation.stop();
  App.scheduler.call(++now);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(static_cast<float>(i));
      automation.stop();
      App.scheduler.call(++now);
    }
    benchmark::DoNotOptimize(automation);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Automation_DelaySchedule);

// --- binary_sensor on_press: [switch.toggle] ---
// Alternating states; the callback returns early on release and calls a no-argument function on press.

static void Automation_BinarySensorOnPress(benchmark::State &state) {
  AutomationBenchSwitch sw;
  setup_switch(sw);
  binary_sensor::BinarySensor binary_sensor;
  setup_binary_sensor(binary_sensor, false);
  binary_sensor.add_on_state_callback([](const bool &state) -> void {
    if (!(state))
      return;
    toggle_no_args_fn();
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
