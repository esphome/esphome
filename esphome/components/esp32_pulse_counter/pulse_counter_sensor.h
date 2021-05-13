#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"

#include <driver/pcnt.h>

namespace esphome {
namespace esp32_pulse_counter {

enum PulseCounterCountMode {
  PULSE_COUNTER_DISABLE = 0,
  PULSE_COUNTER_INCREMENT,
  PULSE_COUNTER_DECREMENT,
};

using pulse_counter_t = int16_t;

struct ESP32PulseCounterStorage {
  bool esp32_pulse_counter_setup(GPIOPin *pin);
  pulse_counter_t read_raw_value();

  static void gpio_intr(ESP32PulseCounterStorage *arg);

  GPIOPin *pin;
  pcnt_unit_t pcnt_unit;
  PulseCounterCountMode rising_edge_mode{PULSE_COUNTER_INCREMENT};
  PulseCounterCountMode falling_edge_mode{PULSE_COUNTER_DISABLE};
  uint32_t filter_us{0};
  pulse_counter_t last_value{0};
};

class ESP32PulseCounterSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_rising_edge_mode(PulseCounterCountMode mode) { storage_.rising_edge_mode = mode; }
  void set_falling_edge_mode(PulseCounterCountMode mode) { storage_.falling_edge_mode = mode; }
  void set_filter_us(uint32_t filter) { storage_.filter_us = filter; }
  void set_total_sensor(sensor::Sensor *total_sensor) { total_sensor_ = total_sensor; }

  /// Unit of measurement is "pulses/min".
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  GPIOPin *pin_;
  ESP32PulseCounterStorage storage_;
  uint32_t current_total_ = 0;
  sensor::Sensor *total_sensor_;
};

extern pcnt_unit_t next_pcnt_unit;

}  // namespace pulse_counter
}  // namespace esphome
