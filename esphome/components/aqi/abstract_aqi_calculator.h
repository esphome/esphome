#pragma once

#include <cstdint>

namespace esphome::aqi {

class AbstractAQICalculator {
 public:
  virtual uint16_t get_aqi(float pm2_5_value, float pm10_0_value) = 0;
};

}  // namespace esphome::aqi
