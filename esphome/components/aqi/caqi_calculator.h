#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include "abstract_aqi_calculator.h"

namespace esphome::aqi {

class CAQICalculator : public AbstractAQICalculator {
 public:
  // The CAQI (CITEAIR) scale defines no maximum: its top "Very high" class is simply ">100". We
  // therefore always extrapolate the top band past 100 without limit, so the extended_range flag
  // (which lifts the AQI calculator's fixed 500 cap) has no meaning here and is ignored.
  uint16_t get_aqi(float pm2_5_value, float pm10_0_value, bool /*extended_range*/) override {
    float pm2_5_index = calculate_index(pm2_5_value, PM2_5_GRID);
    float pm10_0_index = calculate_index(pm10_0_value, PM10_0_GRID);
    float aqi = std::max({pm2_5_index, pm10_0_index, 0.0f});
    aqi = std::min(aqi, static_cast<float>(std::numeric_limits<uint16_t>::max()));
    return static_cast<uint16_t>(std::lround(aqi));
  }

 protected:
  static constexpr int NUM_LEVELS = 4;

  static constexpr int INDEX_GRID[NUM_LEVELS][2] = {{0, 25}, {26, 50}, {51, 75}, {76, 100}};

  static constexpr float PM2_5_GRID[NUM_LEVELS][2] = {
      // clang-format off
      {0.0f, 15.1f},
      {15.1f, 30.1f},
      {30.1f, 55.1f},
      {55.1f, 110.1f}
      // clang-format on
  };

  static constexpr float PM10_0_GRID[NUM_LEVELS][2] = {
      // clang-format off
      {0.0f, 25.1f},
      {25.1f, 50.1f},
      {50.1f, 90.1f},
      {90.1f, 180.1f}
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

    // The top band is open-ended (see get_grid_index), so for concentrations above the last
    // breakpoint this linear fit extrapolates past 100 unbounded, matching CAQI's open ">100" class.
    return (value - conc_lo) * (aqi_hi - aqi_lo) / (conc_hi - conc_lo) + aqi_lo;
  }

  static int get_grid_index(float value, const float array[NUM_LEVELS][2]) {
    for (int i = 0; i < NUM_LEVELS; i++) {
      // The top band is open-ended: any value at or above its lower breakpoint falls into it.
      const bool in_range = (value >= array[i][0]) && (i == NUM_LEVELS - 1 || value < array[i][1]);
      if (in_range) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace esphome::aqi
