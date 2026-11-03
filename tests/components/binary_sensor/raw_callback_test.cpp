#include <gtest/gtest.h>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"

namespace esphome::binary_sensor::testing {

class BinarySensorRawCallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.add_on_raw_state_callback([this](bool value) { this->raw_.push_back(value); });
    this->sensor_.add_on_state_callback([this](bool value) { this->state_.push_back(value); });
  }

  BinarySensor sensor_;
  std::vector<bool> raw_;
  std::vector<bool> state_;
};

TEST_F(BinarySensorRawCallbackTest, FiresOnRepeatedValues) {
  this->sensor_.publish_state(true);
  this->sensor_.publish_state(true);
  this->sensor_.publish_state(false);
  this->sensor_.publish_state(false);
  this->sensor_.invalidate_state();
  EXPECT_EQ(this->raw_, (std::vector<bool>{true, true, false, false}));
  EXPECT_EQ(this->state_, (std::vector<bool>{true, false}));
}

TEST_F(BinarySensorRawCallbackTest, FiresBeforeFilters) {
  InvertFilter invert;
  this->sensor_.add_filter(&invert);
  this->sensor_.publish_state(true);
  this->sensor_.publish_state(true);
  EXPECT_EQ(this->raw_, (std::vector<bool>{true, true}));
  EXPECT_EQ(this->state_, (std::vector<bool>{false}));
}

}  // namespace esphome::binary_sensor::testing
