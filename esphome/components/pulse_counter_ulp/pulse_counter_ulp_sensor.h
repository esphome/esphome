#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "driver/rtc_io.h"  // For gpio_num_t

#include <cinttypes>
#include <chrono>
#include <memory>

namespace esphome {
namespace pulse_counter_ulp {

enum class CountMode { disable = 0, increment = 1, decrement = -1 };

// millis() jumps when a time component synchronises, so we use steady_clock instead
using clock = std::chrono::steady_clock;
using microseconds = std::chrono::duration<uint32_t, std::micro>;

class UlpProgram {
 public:
  struct State {
    uint16_t edge_count;
    uint16_t run_count;
    microseconds mean_exec_time;
  };
  State pop_state();
  State peek_state() const;
  void set_mean_exec_time(microseconds mean_exec_time);

  static std::unique_ptr<UlpProgram> start(gpio_num_t gpio_num, microseconds sleep_duration, CountMode rising_edge_mode,
                                           CountMode falling_edge_mode);
};

class PulseCounterUlpSensor : public sensor::Sensor, public PollingComponent {
 public:
  explicit PulseCounterUlpSensor() {}

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_rising_edge_mode(CountMode mode) { this->rising_edge_mode = mode; }
  void set_falling_edge_mode(CountMode mode) { this->falling_edge_mode = mode; }
  void set_sleep_duration(uint32_t duration_us) { this->sleep_duration_ = duration_us * microseconds{1}; }
  void set_total_sensor(sensor::Sensor *total_sensor) { total_sensor_ = total_sensor; }

  void set_total_pulses(uint32_t pulses);

  /// Unit of measurement is "pulses/min".
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  InternalGPIOPin *pin_;
  CountMode rising_edge_mode{CountMode::increment};
  CountMode falling_edge_mode{CountMode::disable};
  std::unique_ptr<UlpProgram> storage_{};
  clock::time_point last_time_{};
  microseconds sleep_duration_{20000};
  uint32_t current_total_{0};
  sensor::Sensor *total_sensor_{nullptr};
};

}  // namespace pulse_counter_ulp
}  // namespace esphome
