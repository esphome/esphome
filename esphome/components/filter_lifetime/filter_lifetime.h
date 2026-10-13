#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::filter_lifetime {

class FilterLifetime : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void on_shutdown() override;
  void reset_filter();

  void set_max_lifetime(uint32_t minutes) { this->max_lifetime_minutes_ = minutes; }
  void set_is_on_lambda(bool (*is_on)()) { this->is_on_lambda_ = is_on; }
#ifdef USE_BINARY_SENSOR
  void set_is_on_sensor(binary_sensor::BinarySensor *sensor) { this->is_on_sensor_ = sensor; }
#endif
  void set_current_speed_lambda(float (*speed)()) { this->current_speed_lambda_ = speed; }
  void set_current_speed_sensor(sensor::Sensor *sensor) { this->current_speed_sensor_ = sensor; }
  void set_runtime_hours_sensor(sensor::Sensor *sensor) { this->runtime_hours_sensor_ = sensor; }
  void set_remaining_days_sensor(sensor::Sensor *sensor) { this->remaining_days_sensor_ = sensor; }

 protected:
  bool get_is_on_();
  float get_current_speed_();
  void publish_values_();

  bool (*is_on_lambda_)(){nullptr};
  float (*current_speed_lambda_)(){nullptr};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *is_on_sensor_{nullptr};
#endif
  sensor::Sensor *current_speed_sensor_{nullptr};
  sensor::Sensor *runtime_hours_sensor_{nullptr};
  sensor::Sensor *remaining_days_sensor_{nullptr};
  ESPPreferenceObject pref_;
  uint32_t max_lifetime_minutes_{0};
  uint32_t last_update_{0};
  uint32_t runtime_seconds_{0};  // Speed-weighted running time, persisted
  uint32_t carry_ms_{0};         // Weighted milliseconds not yet counted as a second
};

}  // namespace esphome::filter_lifetime
