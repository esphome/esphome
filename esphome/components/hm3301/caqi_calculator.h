#pragma once

#include "esphome/core/log.h"
#include "abstract_aqi_calculator.h"

namespace esphome {
namespace hm3301 {

class CAQICalculator : public AbstractAQICalculator {
 public:
  uint8_t get_aqi(uint16_t pm2_5_value, uint16_t pm10_0_value) override {
    int pm2_5_index = calculate_index_(pm2_5_value, pm2_5_calculation_grid_);
    int pm10_0_index = calculate_index_(pm10_0_value, pm10_0_calculation_grid_);

    return (pm2_5_index < pm10_0_index) ? pm10_0_index : pm2_5_index;
  }

 protected:
  static const int AMOUNT_OF_LEVELS = 5;

  int index_grid_[AMOUNT_OF_LEVELS][2] = {{0, 25}, {26, 50}, {51, 75}, {76, 100}, {101, 400}};

  int pm2_5_calculation_grid_[AMOUNT_OF_LEVELS][2] = {{0, 15}, {16, 30}, {31, 55}, {56, 110}, {111, 400}};

  int pm10_0_calculation_grid_[AMOUNT_OF_LEVELS][2] = {{0, 25}, {26, 50}, {51, 90}, {91, 180}, {181, 400}};

  int calculate_index_(uint16_t value, int array[AMOUNT_OF_LEVELS][2]) {
    int grid_index = get_grid_index_(value, array);
    if (grid_index == -1) {
      return -1;
    }

    int aqi_lo = index_grid_[grid_index][0];
    int aqi_hi = index_grid_[grid_index][1];
    int conc_lo = array[grid_index][0];
    int conc_hi = array[grid_index][1];

    return (value - conc_lo) * (aqi_hi - aqi_lo) / (conc_hi - conc_lo) + aqi_lo;
  }

  int get_grid_index_(uint16_t value, int array[AMOUNT_OF_LEVELS][2]) {
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
