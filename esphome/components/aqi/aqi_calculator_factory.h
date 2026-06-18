#pragma once

#include "aqi_calculator.h"
#include "caqi_calculator.h"
#include "eaqi_calculator.h"

namespace esphome::aqi {

enum AQICalculatorType { CAQI_TYPE = 0, AQI_TYPE = 1, EAQI_TYPE = 2 };

class AQICalculatorFactory {
 public:
  AbstractAQICalculator *get_calculator(AQICalculatorType type) {
    switch (type) {
      case CAQI_TYPE:
        return &this->caqi_calculator_;
      case AQI_TYPE:
        return &this->aqi_calculator_;
      case EAQI_TYPE:
        return &this->eaqi_calculator_;
      default:
        return nullptr;
    }
  }

 protected:
  CAQICalculator caqi_calculator_;
  AQICalculator aqi_calculator_;
  EAQICalculator eaqi_calculator_;
};

}  // namespace esphome::aqi
