#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>
#include <chrono>
#include <memory>

namespace esphome {
namespace pulse_counter_ulp {

enum class CountMode { DISABLE = 0, INCREMENT = 1, DECREMENT = -1 };

using clock = std::chrono::steady_clock;
using microseconds = std::chrono::duration<uint32_t, std::micro>;

class UlpProgram {
 public:
  struct Config {
    InternalGPIOPin *pin_;
    CountMode rising_edge_mode_;
    CountMode falling_edge_mode_;
    microseconds sleep_duration_;
    uint16_t debounce_;
  };
  struct State {
    uint16_t rising_edge_count_;
    uint16_t falling_edge_count_;
    uint16_t run_count_;
    microseconds mean_exec_time_;
  };
  State pop_state();
  State peek_state() const;
  void set_mean_exec_time(microseconds mean_exec_time);

  static std::unique_ptr<UlpProgram> start(const Config &config);
};

class PulseCounterUlpSensor : public sensor::Sensor, public PollingComponent {
 public:
  explicit PulseCounterUlpSensor() {}

  void set_pin(InternalGPIOPin *pin) { this->config_.pin_ = pin; }
  void set_rising_edge_mode(CountMode mode) { this->config_.rising_edge_mode_ = mode; }
  void set_falling_edge_mode(CountMode mode) { this->config_.falling_edge_mode_ = mode; }
  void set_sleep_duration(uint32_t duration_us) { this->config_.sleep_duration_ = duration_us * microseconds{1}; }
  void set_debounce(uint16_t debounce) { this->config_.debounce_ = debounce; }

  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  UlpProgram::Config config_{};
  std::unique_ptr<UlpProgram> storage_{};
  clock::time_point last_time_{};
};

}  // namespace pulse_counter_ulp
}  // namespace esphome
