#pragma once

#include "caqi_calculator.h"
#include "aqi_calculator.h"

namespace esphome::aqi {

enum AQICalculatorType { CAQI_TYPE = 0, AQI_TYPE = 1 };

class AQICalculatorFactory {
 public:
  AbstractAQICalculator *get_calculator(AQICalculatorType type) {
    if (type == 0) {
      return &this->caqi_calculator_;
    } else if (type == 1) {
      return &this->aqi_calculator_;
    }

    return nullptr;
  }

 protected:
  CAQICalculator caqi_calculator_;
  AQICalculator aqi_calculator_;
};

}  // namespace esphome::aqi
