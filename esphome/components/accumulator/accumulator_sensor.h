#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace accumulator {

class AccumulatorSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_sensor(Sensor *sensor) { sensor_ = sensor; }
  void set_reset(bool reset) { reset_ = reset; }
  void set_reset_value(float reset_value) { initial_value_ = reset_value; }

  void set_max_time_interval(int value) { max_value_interval_ = value; }
  void set_min_time_interval(int value) { min_time_interval_ = value; }
  void set_max_value_interval(float value) { max_value_interval_ = value; }

  void reset() { initial_value_ = 0; }

  void process_sensor_value(float value);
  void save_if_needed(float value);

 protected:
  std::string unit_of_measurement() override { return this->sensor_->get_unit_of_measurement(); }
  std::string icon() override { return this->sensor_->get_icon(); }
  int8_t accuracy_decimals() override { return this->sensor_->get_accuracy_decimals() + 2; }

  sensor::Sensor *sensor_;
  ESPPreferenceObject rtc_;

  // settings
  float max_value_interval_;
  int min_time_interval_;
  int max_time_interval_;
  float initial_value_{0.0f};
  bool reset_;

  // Track last save
  float last_saved_value_ = 0;
  uint last_saved_time_ = 0;
};

template<typename... Ts> class ResetAction : public Action<Ts...> {
 public:
  explicit ResetAction(AccumulatorSensor *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset(); }

 protected:
  AccumulatorSensor *parent_;
};

}  // namespace accumulator
}  // namespace esphome
