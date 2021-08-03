#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace integration {

enum IntegrationSensorTime {
  INTEGRATION_SENSOR_TIME_MILLISECOND = 0,
  INTEGRATION_SENSOR_TIME_SECOND,
  INTEGRATION_SENSOR_TIME_MINUTE,
  INTEGRATION_SENSOR_TIME_HOUR,
  INTEGRATION_SENSOR_TIME_DAY,
};

enum IntegrationMethod {
  INTEGRATION_METHOD_TRAPEZOID = 0,
  INTEGRATION_METHOD_LEFT,
  INTEGRATION_METHOD_RIGHT,
};

class IntegrationSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_min_save_interval(uint32_t min_interval) { this->min_save_interval_ = min_interval; }
  void set_sensor(Sensor *sensor) { sensor_ = sensor; }
  void set_time(IntegrationSensorTime time) { time_ = time; }
  void set_method(IntegrationMethod method) { method_ = method; }
  void set_restore(bool restore) { restore_ = restore; }
  void reset() { this->publish_and_save_(0.0f); }

 protected:
  void process_sensor_value_(float value);
  float get_time_factor_() {
    switch (this->time_) {
      case INTEGRATION_SENSOR_TIME_MILLISECOND:
        return 1.0f;
      case INTEGRATION_SENSOR_TIME_SECOND:
        return 1.0f / 1000.0f;
      case INTEGRATION_SENSOR_TIME_MINUTE:
        return 1.0f / 60000.0f;
      case INTEGRATION_SENSOR_TIME_HOUR:
        return 1.0f / 3600000.0f;
      case INTEGRATION_SENSOR_TIME_DAY:
        return 1.0f / 86400000.0f;
      default:
        return 0.0f;
    }
  }
  void publish_and_save_(double result) {
    this->result_ = result;
    this->publish_state(result);
    float result_f = result;
    const uint32_t now = millis();
    if (now - this->last_save_ < this->min_save_interval_)
      return;
    this->last_save_ = now;
    this->rtc_.save(&result_f);
  }
  std::string unit_of_measurement() override;
  std::string icon() override { return this->sensor_->get_icon(); }
  int8_t accuracy_decimals() override { return this->sensor_->get_accuracy_decimals() + 2; }

  sensor::Sensor *sensor_;
  IntegrationSensorTime time_;
  IntegrationMethod method_;
  bool restore_;
  ESPPreferenceObject rtc_;

  uint32_t last_save_{0};
  uint32_t min_save_interval_{0};
  uint32_t last_update_;
  double result_{0.0f};
  float last_value_{0.0f};
};

template<typename... Ts> class ResetAction : public Action<Ts...> {
 public:
  explicit ResetAction(IntegrationSensor *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset(); }

 protected:
  IntegrationSensor *parent_;
};

}  // namespace integration
}  // namespace esphome
