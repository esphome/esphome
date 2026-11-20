#pragma once

#include <gtest/gtest.h>

#include "esphome/components/time_based/cover/time_based_cover.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

namespace esphome::time_based::testing {

class TimeBasedCoverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->open_automation_.add_action(&this->open_action_);
    this->stop_automation_.add_action(&this->stop_action_);
    this->cover_.set_open_duration(10000);
    this->cover_.set_close_duration(10000);
  }

  void move_to_(float position) { this->cover_.make_call().set_position(position).perform(); }

  int open_count_{0};
  int stop_count_{0};
  TimeBasedCover cover_;
  LambdaAction<> open_action_{[this]() { this->open_count_++; }};
  LambdaAction<> stop_action_{[this]() { this->stop_count_++; }};
  Automation<> open_automation_{this->cover_.get_open_trigger()};
  Automation<> stop_automation_{this->cover_.get_stop_trigger()};
};

}  // namespace esphome::time_based::testing
