#pragma once

#include <climits>
#include "abstract_aqi_calculator.h"

// https://document.airnow.gov/technical-assistance-document-for-the-reporting-of-daily-air-quailty.pdf

namespace esphome::aqi {

class AQICalculator : public AbstractAQICalculator {
 public:
  uint16_t get_aqi(uint16_t pm2_5_value, uint16_t pm10_0_value) override {
    int pm2_5_index = calculate_index_(pm2_5_value, PM2_5_GRID);
    int pm10_0_index = calculate_index_(pm10_0_value, PM10_0_GRID);

    return (pm2_5_index < pm10_0_index) ? pm10_0_index : pm2_5_index;
  }

 protected:
  static constexpr int AMOUNT_OF_LEVELS = 6;

  static constexpr int INDEX_GRID[AMOUNT_OF_LEVELS][2] = {{0, 50},    {51, 100},  {101, 150},
                                                          {151, 200}, {201, 300}, {301, 500}};

  static constexpr int PM2_5_GRID[AMOUNT_OF_LEVELS][2] = {{0, 9},    {10, 35},   {36, 55},
                                                          {56, 125}, {126, 225}, {226, INT_MAX}};

  static constexpr int PM10_0_GRID[AMOUNT_OF_LEVELS][2] = {{0, 54},    {55, 154},  {155, 254},
                                                           {255, 354}, {355, 424}, {425, INT_MAX}};

  static int calculate_index_(uint16_t value, const int array[AMOUNT_OF_LEVELS][2]) {
    int grid_index = get_grid_index_(value, array);
    if (grid_index == -1) {
      return -1;
    }
    int aqi_lo = INDEX_GRID[grid_index][0];
    int aqi_hi = INDEX_GRID[grid_index][1];
    int conc_lo = array[grid_index][0];
    int conc_hi = array[grid_index][1];

    return (value - conc_lo) * (aqi_hi - aqi_lo) / (conc_hi - conc_lo) + aqi_lo;
  }

  static int get_grid_index_(uint16_t value, const int array[AMOUNT_OF_LEVELS][2]) {
    for (int i = 0; i < AMOUNT_OF_LEVELS; i++) {
      if (value >= array[i][0] && value <= array[i][1]) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace esphome::aqi
