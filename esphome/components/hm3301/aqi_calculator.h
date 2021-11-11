#pragma once

#ifdef USE_ARDUINO

#include "abstract_aqi_calculator.h"

namespace esphome {
namespace hm3301 {

class AQICalculator : public AbstractAQICalculator {
 public:
  uint8_t get_aqi(uint16_t pm10_value) override {
    return calculate_aqi_index_(pm10_value, pm10_concentration_breakpoints_);
  }

 protected:
  static const int AMOUNT_OF_LEVELS = 6;

  int aqi_index_[AMOUNT_OF_LEVELS][2] = {{0, 51}, {51, 100}, {101, 150}, {151, 200}, {201, 300}, {301, 500}};

  int pm10_concentration_breakpoints_[AMOUNT_OF_LEVELS][2] = {{0, 54},    {55, 154},  {155, 254},
                                                              {255, 354}, {355, 424}, {425, 604}};

  int calculate_aqi_index_(uint16_t value, int array[AMOUNT_OF_LEVELS][2]) {
    int grid_index = get_concentration_breakpoint_level_(value, array);
    int aqi_lo = aqi_index_[grid_index][0];
    int aqi_hi = aqi_index_[grid_index][1];
    int conc_lo = array[grid_index][0];
    int conc_hi = array[grid_index][1];

    return ((aqi_hi - aqi_lo) / (conc_hi - conc_lo)) * (value - conc_lo) + aqi_lo;
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
