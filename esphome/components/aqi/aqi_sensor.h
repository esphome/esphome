#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "aqi_calculator_factory.h"

namespace esphome::aqi {

class AQISensor final : public sensor::Sensor, public Component {
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)

 public:
  void setup() override;
  void dump_config() override;

  void set_aqi_calculation_type(AQICalculatorType type) { this->aqi_calc_type_ = type; }
  void set_extended_range(bool extended_range) { this->extended_range_ = extended_range; }

 protected:
  void calculate_aqi_();

  AQICalculatorType aqi_calc_type_{AQI_TYPE};
  bool extended_range_{false};
  AQICalculatorFactory aqi_calculator_factory_;

  float pm_2_5_value_{NAN};
  float pm_10_0_value_{NAN};
};

}  // namespace esphome::aqi
