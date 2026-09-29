#pragma once

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

#include "esphome/components/exponential_moving_average/exponential_moving_average_sensor.h"
#include "esphome/components/host/preferences.h"

namespace esphome::exponential_moving_average::testing {

class TestableExponentialMovingAverageSensor : public ExponentialMovingAverageSensor {
 public:
  using ExponentialMovingAverageSensor::ExponentialMovingAverageSensor;
  using ExponentialMovingAverageSensor::process_;
};

// Unnamed sensors share one preference key, so a second instance created after
// the first one behaves like the same sensor after a reboot.
class ExponentialMovingAverageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Keep preferences away from the user's home directory.
    setenv("ESPHOME_PREFDIR", std::filesystem::temp_directory_path().c_str(), 1);
    host::setup_preferences();
    global_preferences->reset();
  }

  sensor::Sensor source_;
};

}  // namespace esphome::exponential_moving_average::testing
