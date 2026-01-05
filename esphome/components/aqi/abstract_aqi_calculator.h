#pragma once

#include <cstdint>

namespace esphome::aqi {

class AbstractAQICalculator {
 public:
  virtual uint16_t get_aqi(uint16_t pm2_5_value, uint16_t pm10_0_value) = 0;
};

}  // namespace esphome::aqi
