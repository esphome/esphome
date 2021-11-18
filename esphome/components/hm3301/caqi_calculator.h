#pragma once

#include "abstract_aqi_calculator.h"

namespace esphome {
namespace hm3301 {

class CAQICalculator : public AbstractAQICalculator {
 public:
  uint8_t get_aqi(uint16_t pm2_5_value, uint16_t pm10_0_value) override {
    return calculate_index_(pm10_0_value, pm10_0_concentration_breakpoints_);
  }

 protected:
  static const int AMOUNT_OF_LEVELS = 5;

  int aqi_index_[AMOUNT_OF_LEVELS][2] = {{0, 25}, {26, 50}, {51, 75}, {76, 100}, {101, 400}};

  int pm10_0_concentration_breakpoints_[AMOUNT_OF_LEVELS][2] = {{0, 25}, {26, 50}, {51, 90}, {91, 180}, {181, 400}};

  int calculate_index_(uint16_t value, int concentration_breakpoints[AMOUNT_OF_LEVELS][2]) {
    int breakpoint_index = get_breakpoint_index_(value, concentration_breakpoints);
    if (breakpoint_index == -1) {
      return -1;
    }

    int aqi_lo = aqi_index_[breakpoint_index][0];
    int aqi_hi = aqi_index_[breakpoint_index][1];
    int conc_lo = concentration_breakpoints[breakpoint_index][0];
    int conc_hi = concentration_breakpoints[breakpoint_index][1];

    return (value - conc_lo) * (aqi_hi - aqi_lo) / (conc_hi - conc_lo) + aqi_lo;
  }

  int get_breakpoint_index_(uint16_t value, int concentration_breakpoints[AMOUNT_OF_LEVELS][2]) {
    for (int i = 0; i < AMOUNT_OF_LEVELS; i++) {
      if (value >= concentration_breakpoints[i][0] && value <= concentration_breakpoints[i][1]) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace hm3301
}  // namespace esphome
