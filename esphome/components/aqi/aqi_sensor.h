#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "aqi_calculator_factory.h"

namespace esphome::aqi {

class AQISensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_pm_2_5_sensor(sensor::Sensor *sensor) { this->pm_2_5_sensor_ = sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *sensor) { this->pm_10_0_sensor_ = sensor; }
  void set_aqi_calculation_type(AQICalculatorType type) { this->aqi_calc_type_ = type; }

 protected:
  void calculate_aqi_();

  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  AQICalculatorType aqi_calc_type_{AQI_TYPE};
  AQICalculatorFactory aqi_calculator_factory_;

  float pm_2_5_value_{NAN};
  float pm_10_0_value_{NAN};
};

}  // namespace esphome::aqi
