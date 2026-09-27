#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "esphome/components/counter/counter_sensor.h"

namespace esphome::counter::testing {

constexpr int64_t INT64_MAX_VALUE = std::numeric_limits<int64_t>::max();
constexpr int64_t INT64_MIN_VALUE = std::numeric_limits<int64_t>::min();

/// Stands in for a binary sensor: reports each state to its callbacks.
struct FakeBinarySource {
  template<typename F> void add_on_state_callback(F &&callback) { this->callbacks_.add(std::forward<F>(callback)); }
  void publish(bool state) { this->callbacks_.call(state); }
  CallbackManager<void(bool)> callbacks_;
};

// Restore is off so no preference storage is needed.
class CounterTest : public ::testing::Test {
 protected:
  CounterSensor counter_{false, 0};
};

}  // namespace esphome::counter::testing
