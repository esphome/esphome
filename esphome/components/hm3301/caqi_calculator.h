#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "abstract_aqi_calculator.h"

namespace esphome {
namespace hm3301 {

class CAQICalculator : public AbstractAQICalculator {
 public:
  uint8_t get_aqi(uint16_t pm10_value) override {
    return calculate_aqi_index_(pm10_value, pm10_concentration_breakpoints_);
  }

 protected:
  static const int AMOUNT_OF_LEVELS = 5;

  int aqi_index_[AMOUNT_OF_LEVELS][2] = {{0, 25}, {26, 50}, {51, 75}, {76, 100}, {101, 400}};

  int pm10_concentration_breakpoints_[AMOUNT_OF_LEVELS][2] = {{0, 25}, {26, 50}, {51, 90}, {91, 180}, {181, 400}};

  int calculate_aqi_index_(uint16_t value, int array[AMOUNT_OF_LEVELS][2]) {
    int conc_breakpoint_level = get_concentration_breakpoint_level_(value, array);
    if (conc_breakpoint_level == -1) {
      return -1;
    }

    int aqi_lo = aqi_index_[conc_breakpoint_level][0];
    int aqi_hi = aqi_index_[conc_breakpoint_level][1];
    int conc_lo = array[conc_breakpoint_level][0];
    int conc_hi = array[conc_breakpoint_level][1];

    int aqi = ((aqi_hi - aqi_lo) / (conc_hi - conc_lo)) * (value - conc_lo) + aqi_lo;

    return aqi;
  }

  int get_concentration_breakpoint_level_(uint16_t value, int array[AMOUNT_OF_LEVELS][2]) {
    for (int i = 0; i < AMOUNT_OF_LEVELS; i++) {
      if (value >= array[i][0] && value <= array[i][1]) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace hm3301
}  // namespace esphome

#endif  // USE_ARDUINO
