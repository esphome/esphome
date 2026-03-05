#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include "abstract_aqi_calculator.h"

namespace esphome::aqi {

class CAQICalculator : public AbstractAQICalculator {
 public:
  uint16_t get_aqi(float pm2_5_value, float pm10_0_value) override {
    float pm2_5_index = calculate_index(pm2_5_value, PM2_5_GRID);
    float pm10_0_index = calculate_index(pm10_0_value, PM10_0_GRID);

    float aqi = std::max(pm2_5_index, pm10_0_index);
    if (aqi < 0.0f) {
      aqi = 0.0f;
    }
    return static_cast<uint16_t>(std::lround(aqi));
  }

 protected:
  static constexpr int NUM_LEVELS = 5;

  static constexpr int INDEX_GRID[NUM_LEVELS][2] = {{0, 25}, {26, 50}, {51, 75}, {76, 100}, {101, 400}};

  static constexpr float PM2_5_GRID[NUM_LEVELS][2] = {
      // clang-format off
      {0.0f, 15.1f},
      {15.1f, 30.1f},
      {30.1f, 55.1f},
      {55.1f, 110.1f},
      {110.1f, std::numeric_limits<float>::max()}
      // clang-format on
  };

  static constexpr float PM10_0_GRID[NUM_LEVELS][2] = {
      // clang-format off
      {0.0f, 25.1f},
      {25.1f, 50.1f},
      {50.1f, 90.1f},
      {90.1f, 180.1f},
      {180.1f, std::numeric_limits<float>::max()}
      // clang-format on
  };

  static float calculate_index(float value, const float array[NUM_LEVELS][2]) {
    int grid_index = get_grid_index(value, array);
    if (grid_index == -1) {
      return -1.0f;
    }

    float aqi_lo = INDEX_GRID[grid_index][0];
    float aqi_hi = INDEX_GRID[grid_index][1];
    float conc_lo = array[grid_index][0];
    float conc_hi = array[grid_index][1];

    return (value - conc_lo) * (aqi_hi - aqi_lo) / (conc_hi - conc_lo) + aqi_lo;
  }

  static int get_grid_index(float value, const float array[NUM_LEVELS][2]) {
    for (int i = 0; i < NUM_LEVELS; i++) {
      const bool in_range =
          (value >= array[i][0]) && ((i == NUM_LEVELS - 1) ? (value <= array[i][1])   // last bucket inclusive
                                                           : (value < array[i][1]));  // others exclusive on hi
      if (in_range) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace esphome::aqi
