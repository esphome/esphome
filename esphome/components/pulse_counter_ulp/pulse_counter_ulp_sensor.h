#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#include <cinttypes>

namespace esphome {
namespace pulse_counter_ulp {

enum PulseCounterCountMode {
  PULSE_COUNTER_DISABLE = 0,
  PULSE_COUNTER_INCREMENT,
  PULSE_COUNTER_DECREMENT,
};

using pulse_counter_t = int32_t;
using timestamp_t = int64_t;

struct UlpPulseCounterStorage {
  bool pulse_counter_setup(InternalGPIOPin *pin);
  pulse_counter_t read_raw_value();

  InternalGPIOPin *pin;
  PulseCounterCountMode rising_edge_mode{PULSE_COUNTER_INCREMENT};
  PulseCounterCountMode falling_edge_mode{PULSE_COUNTER_DISABLE};
  pulse_counter_t last_value{0};
};

class PulseCounterUlpSensor : public sensor::Sensor, public PollingComponent {
 public:
  explicit PulseCounterUlpSensor() {}

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_rising_edge_mode(PulseCounterCountMode mode) { storage_.rising_edge_mode = mode; }
  void set_falling_edge_mode(PulseCounterCountMode mode) { storage_.falling_edge_mode = mode; }
  void set_total_sensor(sensor::Sensor *total_sensor) { total_sensor_ = total_sensor; }
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { time_id_ = time_id; }
#endif

  void set_total_pulses(uint32_t pulses);

  /// Unit of measurement is "pulses/min".
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  InternalGPIOPin *pin_;
  UlpPulseCounterStorage storage_;
  timestamp_t last_time_{0};
  uint32_t current_total_{0};
  sensor::Sensor *total_sensor_{nullptr};
#ifdef USE_TIME
  time::RealTimeClock *time_id_{nullptr};
  bool time_is_synchronized_{false};
  // Store last_time_ across deep sleep
  ESPPreferenceObject pref_{};
#endif
};

}  // namespace pulse_counter_ulp
}  // namespace esphome
