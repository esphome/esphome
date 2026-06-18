#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include "abstract_aqi_calculator.h"

namespace esphome::aqi {

class EAQICalculator : public AbstractAQICalculator {
 public:
  uint16_t get_aqi(float pm2_5_value, float pm10_0_value) override {
    float indices[2] = {
        calculate_index(pm2_5_value, PM2_5_GRID),
        calculate_index(pm10_0_value, PM10_0_GRID),
    };
    return static_cast<uint16_t>(std::lround(*std::max_element(indices, indices + 2)));
  }

 protected:
  static constexpr int NUM_LEVELS = 6;

  static constexpr int INDEX_GRID[NUM_LEVELS][2] = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}};

  static constexpr float PM2_5_GRID[NUM_LEVELS][2] = {
      {0.0f, 5.0f},   {5.0f, 15.0f},   {15.0f, 50.0f},
      {50.0f, 90.0f}, {90.0f, 140.0f}, {140.0f, std::numeric_limits<float>::max()},
  };

  static constexpr float PM10_0_GRID[NUM_LEVELS][2] = {
      {0.0f, 15.0f},    {15.0f, 45.0f},   {45.0f, 120.0f},
      {120.0f, 195.0f}, {195.0f, 270.0f}, {270.0f, std::numeric_limits<float>::max()},
  };

  static float calculate_index(float value, const float array[NUM_LEVELS][2]) {
    if (value < 0.0f) {
      return 0.0f;
    }

    int grid_index = get_grid_index(value, array);
    if (grid_index == -1) {
      return 0.0f;
    }

    float aqi_lo = static_cast<float>(INDEX_GRID[grid_index][0]);
    float aqi_hi = static_cast<float>(INDEX_GRID[grid_index][1]);
    float conc_lo = array[grid_index][0];
    float conc_hi = array[grid_index][1];

    if (conc_hi == conc_lo) {
      return aqi_lo;
    }
    return (value - conc_lo) * (aqi_hi - aqi_lo) / (conc_hi - conc_lo) + aqi_lo;
  }

  static int get_grid_index(float value, const float array[NUM_LEVELS][2]) {
    for (int i = 0; i < NUM_LEVELS; i++) {
      bool in_range = value >= array[i][0] && value <= array[i][1];
      if (in_range) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace esphome::aqi
