#include <gtest/gtest.h>
#include "esphome/components/ld2420/ld2420.h"

namespace esphome::ld2420::testing {

class TestNumber : public number::Number {
 protected:
  void control(float value) override {}
};

class LD2420GateThresholdTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (uint8_t gate = 0; gate < TOTAL_GATES; gate++) {
      this->ld2420_.new_config.move_thresh[gate] = 1000 + gate;
      this->ld2420_.new_config.still_thresh[gate] = 2000 + gate;
    }
  }
  void expect_untouched_except_(const uint32_t *thresh, uint32_t base, int skip) {
    for (int gate = 0; gate < TOTAL_GATES; gate++) {
      if (gate != skip)
        EXPECT_EQ(thresh[gate], base + gate) << "gate " << gate;
    }
  }

  LD2420Component ld2420_;
  TestNumber move_;
  TestNumber still_;
};

TEST_F(LD2420GateThresholdTest, PerGateNumbersWriteTheirOwnGate) {
  this->ld2420_.set_gate_move_threshold_numbers(3, &this->move_);
  this->ld2420_.set_gate_still_threshold_numbers(3, &this->still_);

  this->ld2420_.set_gate_threshold_value(&this->move_, 500);
  EXPECT_EQ(this->ld2420_.new_config.move_thresh[3], 500u);
  this->expect_untouched_except_(this->ld2420_.new_config.move_thresh, 1000, 3);
  this->expect_untouched_except_(this->ld2420_.new_config.still_thresh, 2000, -1);

  this->ld2420_.set_gate_threshold_value(&this->still_, 750);
  EXPECT_EQ(this->ld2420_.new_config.still_thresh[3], 750u);
  EXPECT_EQ(this->ld2420_.new_config.move_thresh[3], 500u);
  this->expect_untouched_except_(this->ld2420_.new_config.still_thresh, 2000, 3);
}

TEST_F(LD2420GateThresholdTest, GateSelectWritesTheSelectedGate) {
  TestNumber select;
  this->ld2420_.set_gate_select_number(&select);
  this->ld2420_.set_gate_move_threshold_numbers(0, &this->move_);
  this->ld2420_.set_gate_still_threshold_numbers(0, &this->still_);
  select.publish_state(5);

  this->ld2420_.set_gate_threshold_value(&this->move_, 300);
  this->ld2420_.set_gate_threshold_value(&this->still_, 400);
  EXPECT_EQ(this->ld2420_.new_config.move_thresh[5], 300u);
  EXPECT_EQ(this->ld2420_.new_config.still_thresh[5], 400u);
  this->expect_untouched_except_(this->ld2420_.new_config.move_thresh, 1000, 5);
  this->expect_untouched_except_(this->ld2420_.new_config.still_thresh, 2000, 5);
}

TEST_F(LD2420GateThresholdTest, UnregisteredNumberChangesNothing) {
  this->ld2420_.set_gate_move_threshold_numbers(3, &this->move_);
  this->ld2420_.set_gate_still_threshold_numbers(3, &this->still_);
  TestNumber other;

  this->ld2420_.set_gate_threshold_value(&other, 123);
  this->expect_untouched_except_(this->ld2420_.new_config.move_thresh, 1000, -1);
  this->expect_untouched_except_(this->ld2420_.new_config.still_thresh, 2000, -1);
}

}  // namespace esphome::ld2420::testing
