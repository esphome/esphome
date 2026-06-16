#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include "abstract_aqi_calculator.h"

namespace esphome::aqi {

class EAQICalculator : public AbstractAQICalculator {
 public:
  // Updated signature to accept all 5 pollutants required for EAQI
  // Note: You may need to modify abstract_aqi_calculator.h to support 5 arguments
  // or create a wrapper that calls this method.
  uint16_t calculate_eaqi(float pm2_5, float pm10, float o3, float no2, float so2) {
    float indices[5];

    indices[0] = calculate_index(pm2_5, PM2_5_GRID);
    indices[1] = calculate_index(pm10, PM10_GRID);
    indices[2] = calculate_index(o3, O3_GRID);
    indices[3] = calculate_index(no2, NO2_GRID);
    indices[4] = calculate_index(so2, SO2_GRID);

    float max_index = 0.0f;
    for (int i = 0; i < 5; i++) {
      if (indices[i] > max_index) {
        max_index = indices[i];
      }
    }

    // Round to nearest integer (1-6)
    return static_cast<uint16_t>(std::lround(max_index));
  }

 protected:
  static constexpr int NUM_LEVELS = 6;  // EAQI has 6 levels

  // Index Grid: Since each level maps to a single integer, lo == hi
  // Levels: 1=Good, 2=Fair, 3=Moderate, 4=Poor, 5=Very Poor, 6=Extremely Poor
  static constexpr int INDEX_GRID[NUM_LEVELS][2] = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}};

  static constexpr float PM2_5_GRID[NUM_LEVELS][2] = {{0.0f, 5.0f},    {5.0f, 15.0f},
                                                      {15.0f, 50.0f},  {50.0f, 90.0f},
                                                      {90.0f, 140.0f}, {140.0f, std::numeric_limits<float>::max()}};

  static constexpr float PM10_GRID[NUM_LEVELS][2] = {{0.0f, 15.0f},    {15.0f, 45.0f},
                                                     {45.0f, 120.0f},  {120.0f, 195.0f},
                                                     {195.0f, 270.0f}, {270.0f, std::numeric_limits<float>::max()}};

  static constexpr float O3_GRID[NUM_LEVELS][2] = {{0.0f, 60.0f},    {60.0f, 100.0f},
                                                   {100.0f, 120.0f}, {120.0f, 160.0f},
                                                   {160.0f, 180.0f}, {180.0f, std::numeric_limits<float>::max()}};

  static constexpr float NO2_GRID[NUM_LEVELS][2] = {{0.0f, 10.0f},    {10.0f, 25.0f},
                                                    {25.0f, 60.0f},   {60.0f, 100.0f},
                                                    {100.0f, 150.0f}, {150.0f, std::numeric_limits<float>::max()}};

  static constexpr float SO2_GRID[NUM_LEVELS][2] = {{0.0f, 20.0f},    {20.0f, 40.0f},
                                                    {40.0f, 125.0f},  {125.0f, 190.0f},
                                                    {190.0f, 275.0f}, {275.0f, std::numeric_limits<float>::max()}};

  static float calculate_index(float value, const float array[NUM_LEVELS][2]) {
    if (value < 0)
      return 0.0f;

    int grid_index = get_grid_index(value, array);
    if (grid_index == -1) {
      return 0.0f;
    }

    float aqi_lo = static_cast<float>(INDEX_GRID[grid_index][0]);
    float aqi_hi = static_cast<float>(INDEX_GRID[grid_index][1]);
    float conc_lo = array[grid_index][0];
    float conc_hi = array[grid_index][1];

    if (conc_hi == conc_lo)
      return aqi_lo;

    return (value - conc_lo) * (aqi_hi - aqi_lo) / (conc_hi - conc_lo) + aqi_lo;
  }

  static int get_grid_index(float value, const float array[NUM_LEVELS][2]) {
    for (int i = 0; i < NUM_LEVELS; i++) {
      bool in_range = (value >= array[i][0]) && ((i == NUM_LEVELS - 1) ? true : (value < array[i][1]));

      if (in_range) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace esphome::aqi
