// Filter Lifetime Component
// Tracks filter usage time based on runtime and speed settings
// Calculates remaining filter lifetime as a percentage

#pragma once
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include <functional>

namespace esphome {
namespace filter_lifetime {

class FilterLifetime : public sensor::Sensor, public PollingComponent {
 public:
  void reset_filter();
  void dump_config() override;
  void setup() override;
  void update() override;

  void set_max_lifetime(int max_lifetime) { this->max_lifetime_ = max_lifetime; }
  void set_is_on_lambda(std::function<bool()> is_on) { this->is_on_lambda_ = std::move(is_on); }
  void set_is_on_sensor(binary_sensor::BinarySensor *sensor) { this->is_on_sensor_ = sensor; }
  void set_current_speed_lambda(std::function<float()> current_speed) {
    this->current_speed_lambda_ = std::move(current_speed);
  }
  void set_current_speed_sensor(sensor::Sensor *sensor) { this->current_speed_sensor_ = sensor; }
  void set_runtime_hours_sensor(sensor::Sensor *sensor) { this->runtime_hours_sensor_ = sensor; }
  void set_remaining_days_sensor(sensor::Sensor *sensor) { this->remaining_days_sensor_ = sensor; }

 protected:
  bool get_is_on_();
  float get_current_speed_();

  int max_lifetime_{0};                                 // Maximum filter lifetime in months
  std::function<bool()> is_on_lambda_;                  // Lambda to check if device is on
  binary_sensor::BinarySensor *is_on_sensor_{nullptr};  // Binary sensor to check if device is on
  std::function<float()> current_speed_lambda_;         // Lambda to get current speed percentage (0-100)
  sensor::Sensor *current_speed_sensor_{nullptr};       // Sensor to get current speed percentage (0-100)
  uint32_t last_update_{0};                             // Timestamp of last update (milliseconds)
  float runtime_minutes_{0.0f};                         // Total runtime in minutes (persisted to flash)
  ESPPreferenceObject pref_;                            // Preference storage object
  sensor::Sensor *runtime_hours_sensor_{nullptr};       // Optional: Runtime hours sensor
  sensor::Sensor *remaining_days_sensor_{nullptr};      // Optional: Remaining days sensor
};

}  // namespace filter_lifetime
}  // namespace esphome
