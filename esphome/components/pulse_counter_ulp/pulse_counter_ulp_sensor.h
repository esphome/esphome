#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>
#include <chrono>

namespace esphome {
namespace pulse_counter_ulp {

enum class CountMode { disable = 0, increment = 1, decrement = -1 };

// millis() jumps when a time component synchronises, so we use steady_clock instead
using clock = std::chrono::steady_clock;

class UlpProgram {
 public:
  struct state {
    uint16_t edge_count;
    uint16_t run_count;
  };
  bool setup(InternalGPIOPin *pin);
  state pop_state();
  state peek_state() const;

  InternalGPIOPin *pin;
  CountMode rising_edge_mode{CountMode::increment};
  CountMode falling_edge_mode{CountMode::disable};
  std::chrono::duration<uint32_t, std::micro> sleep_duration_{20000};

 private:
  bool setup_ulp();
};

class PulseCounterUlpSensor : public sensor::Sensor, public PollingComponent {
 public:
  explicit PulseCounterUlpSensor() {}

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_rising_edge_mode(CountMode mode) { storage_.rising_edge_mode = mode; }
  void set_falling_edge_mode(CountMode mode) { storage_.falling_edge_mode = mode; }
  void set_sleep_duration(uint32_t duration_us) {
    storage_.sleep_duration_ = std::chrono::microseconds{1} * duration_us;
    this->ulp_mean_exec_time_ = duration_us * std::chrono::microseconds{1};
  }
  void set_total_sensor(sensor::Sensor *total_sensor) { total_sensor_ = total_sensor; }

  void set_total_pulses(uint32_t pulses);

  /// Unit of measurement is "pulses/min".
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  InternalGPIOPin *pin_;
  UlpProgram storage_;
  clock::time_point last_time_{};
  std::chrono::duration<float> ulp_mean_exec_time_{};
  uint32_t current_total_{0};
  sensor::Sensor *total_sensor_{nullptr};
};

}  // namespace pulse_counter_ulp
}  // namespace esphome
