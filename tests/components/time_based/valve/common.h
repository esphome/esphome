#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "esphome/components/time_based/valve/time_based_valve.h"

namespace esphome::time_based::testing {

// Expose protected members for testing.
class TestableTimeBasedValve : public TimeBasedValve {
 public:
  using TimeBasedValve::target_position_;
  using TimeBasedValve::last_recompute_time_;
  using TimeBasedValve::current_operation;
  using TimeBasedValve::measured_position_;
  uint32_t mock_millis = 0;

 protected:
  uint32_t get_millis() override { return mock_millis; }
};

}  // namespace esphome::time_based::testing
