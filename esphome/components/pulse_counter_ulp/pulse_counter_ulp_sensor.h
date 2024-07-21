#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>

namespace esphome {
namespace pulse_counter_ulp {

enum class CountMode { disable = 0, increment = 1, decrement = -1 };

using timestamp_t = int64_t;

struct UlpProgram {
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
};

class PulseCounterUlpSensor : public sensor::Sensor, public PollingComponent {
 public:
  explicit PulseCounterUlpSensor() {}

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_rising_edge_mode(CountMode mode) { storage_.rising_edge_mode = mode; }
  void set_falling_edge_mode(CountMode mode) { storage_.falling_edge_mode = mode; }
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
  timestamp_t last_time_{0};
  uint32_t current_total_{0};
  sensor::Sensor *total_sensor_{nullptr};
};

}  // namespace pulse_counter_ulp
}  // namespace esphome
