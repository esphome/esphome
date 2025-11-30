#pragma once

#include <cstdint>

namespace esphome {
namespace aqi {

class AbstractAQICalculator {
 public:
  virtual uint16_t get_aqi(uint16_t pm2_5_value, uint16_t pm10_0_value) = 0;
};

}  // namespace aqi
}  // namespace esphome
