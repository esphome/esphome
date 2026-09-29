#pragma once

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include "esphome/components/exponential_moving_average/exponential_moving_average_sensor.h"
#include "esphome/core/preferences.h"
#ifdef USE_HOST
#include "esphome/components/host/preferences.h"
#endif

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
    if (const char *prefdir = getenv("ESPHOME_PREFDIR"); prefdir != nullptr)
      this->saved_prefdir_ = prefdir;
    // Keep preferences away from the user's home directory.
    setenv("ESPHOME_PREFDIR", std::filesystem::temp_directory_path().c_str(), 1);
#ifdef USE_HOST
    host::setup_preferences();
#endif
    global_preferences->reset();
  }

  void TearDown() override {
    global_preferences->reset();
    if (this->saved_prefdir_.has_value()) {
      setenv("ESPHOME_PREFDIR", this->saved_prefdir_->c_str(), 1);
    } else {
      unsetenv("ESPHOME_PREFDIR");
    }
  }

  std::optional<std::string> saved_prefdir_;
  sensor::Sensor source_;
};

}  // namespace esphome::exponential_moving_average::testing
