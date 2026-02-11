#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::pm100x {

enum class PM100XModel {
  PM1003,
  PM1006,
  PM1006K,
};

// Base class for PM100X sensors (shared logic for PWM and UART modes)
class PM100XComponent : public PollingComponent {
 public:
  PM100XComponent() = default;

  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { this->pm_2_5_sensor_ = pm_2_5_sensor; }
  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0_sensor) { this->pm_1_0_sensor_ = pm_1_0_sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { this->pm_10_0_sensor_ = pm_10_0_sensor; }
  void set_model(PM100XModel model) { this->model_ = model; }
  void set_startup_delay(uint32_t delay_ms) { this->startup_delay_ms_ = delay_ms; }
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  float duty_to_pm25_(float duty_percent) const;

  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};

  uint32_t start_time_{0};
  bool initial_delay_done_{false};
  uint32_t startup_delay_ms_{15000};
  PM100XModel model_{PM100XModel::PM1003};
};

}  // namespace esphome::pm100x
