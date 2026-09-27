#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "esphome/components/counter/counter_sensor.h"

namespace esphome::counter::testing {

constexpr int64_t INT64_MAX_VALUE = std::numeric_limits<int64_t>::max();
constexpr int64_t INT64_MIN_VALUE = std::numeric_limits<int64_t>::min();

// Restore is off so no preference storage is needed.
class CounterTest : public ::testing::Test {
 protected:
  CounterSensor counter_{false};
};

}  // namespace esphome::counter::testing
