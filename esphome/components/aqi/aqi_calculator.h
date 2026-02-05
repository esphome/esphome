#pragma once

#include <cmath>
#include <limits>
#include "abstract_aqi_calculator.h"

// https://document.airnow.gov/technical-assistance-document-for-the-reporting-of-daily-air-quailty.pdf
// According to this document, in order to use these breakpoints (so that they are valid), the input values must be
// truncated as follows:
// PM 2.5 (μg/m3) – truncate to 1 decimal place
// PM 10 (μg/m3) – truncate to integer

namespace esphome::aqi {

static float trunc_1dp(float x) {
  if (!std::isfinite(x) || x < 0.0f) {
    return 0.0f;
  }
  return std::floor(x * 10.0f) / 10.0f;  // truncate to 0.1
}

static float trunc_int(float x) {
  if (!std::isfinite(x) || x < 0.0f) {
    return 0.0f;
  }
  return std::floor(x);  // truncate to integer
}

class AQICalculator : public AbstractAQICalculator {
 public:
  uint16_t get_aqi(float pm2_5_value, float pm10_0_value) override {
    pm2_5_value = trunc_1dp(pm2_5_value);
    pm10_0_value = trunc_int(pm10_0_value);
    float pm2_5_index = calculate_index(pm2_5_value, PM2_5_GRID);
    float pm10_0_index = calculate_index(pm10_0_value, PM10_0_GRID);

    return static_cast<uint16_t>(std::round(std::max(pm2_5_index, pm10_0_index)));
  }

 protected:
  static constexpr int NUM_LEVELS = 6;

  static constexpr int INDEX_GRID[NUM_LEVELS][2] = {{0, 50}, {51, 100}, {101, 150}, {151, 200}, {201, 300}, {301, 500}};

  static constexpr float PM2_5_GRID[NUM_LEVELS][2] = {{0.0f, 9.0f},     {9.1f, 35.4f},
                                                      {35.5f, 55.4f},   {55.5f, 125.4f},
                                                      {125.5f, 225.4f}, {225.5f, std::numeric_limits<float>::max()}};

  static constexpr float PM10_0_GRID[NUM_LEVELS][2] = {{0.0f, 54.0f},    {55.0f, 154.0f},
                                                       {155.0f, 254.0f}, {255.0f, 354.0f},
                                                       {355.0f, 424.0f}, {425.0f, std::numeric_limits<float>::max()}};

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
      if (value >= array[i][0] && value <= array[i][1]) {
        return i;
      }
    }
    return -1;
  }
};

}  // namespace esphome::aqi
