#pragma once

#include "abstract_aqi_calculator.h"

namespace esphome {
namespace hm3301 {

class AQICalculator : public AbstractAQICalculator {
 public:
  uint8_t get_aqi(uint16_t pm2_5_value, uint16_t pm10_0_value) override {
    int pm2_5_index = calculate_index_(pm2_5_value, pm2_5_calculation_grid_);
    int pm10_0_index = calculate_index_(pm10_0_value, pm10_0_concentration_breakpoints_);

    return (pm2_5_index < pm10_0_index) ? pm10_0_index : pm2_5_index;
  }

 protected:
  static const int AMOUNT_OF_LEVELS = 6;

  int aqi_index_[AMOUNT_OF_LEVELS][2] = {{0, 51}, {51, 100}, {101, 150}, {151, 200}, {201, 300}, {301, 500}};

  int pm2_5_calculation_grid_[AMOUNT_OF_LEVELS][2] = {{0, 12}, {13, 35}, {36, 55}, {56, 150}, {151, 250}, {251, 500}};

  int pm10_0_concentration_breakpoints_[AMOUNT_OF_LEVELS][2] = {{0, 54},    {55, 154},  {155, 254},
                                                       {255, 354}, {355, 424}, {425, 604}};

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
