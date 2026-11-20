#include "../common.h"

namespace esphome::time_based::testing {

using cover::COVER_OPEN;
using cover::COVER_OPERATION_OPENING;

TEST_F(TimeBasedCoverTest, SameDirectionIsNotRerunByDefault) {
  this->cover_.position = 0.5f;
  this->move_to_(0.8f);
  this->move_to_(COVER_OPEN);

  EXPECT_EQ(this->open_count_, 1);
  EXPECT_EQ(this->stop_count_, 0);
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_OPENING);
}

TEST_F(TimeBasedCoverTest, SameDirectionIsRerunWhenEnabled) {
  this->cover_.set_rerun_action_while_moving(true);
  this->cover_.position = 0.5f;
  this->move_to_(0.8f);
  this->move_to_(COVER_OPEN);

  EXPECT_EQ(this->open_count_, 2);
  EXPECT_EQ(this->stop_count_, 0);
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_OPENING);
}

// With manual_control and has_built_in_endstop, only one "already at target" branch may start the cover,
// or rerun_action_while_moving would run open_action twice for one request.
TEST_F(TimeBasedCoverTest, OpenAtTargetWithManualControlAndEndstopRunsOnce) {
  this->cover_.set_manual_control(true);
  this->cover_.set_has_built_in_endstop(true);
  this->cover_.set_rerun_action_while_moving(true);
  this->cover_.position = COVER_OPEN;
  this->move_to_(COVER_OPEN);

  EXPECT_EQ(this->open_count_, 1);
  EXPECT_EQ(this->stop_count_, 0);
  EXPECT_EQ(this->cover_.current_operation, COVER_OPERATION_OPENING);
}

TEST_F(TimeBasedCoverTest, OpenAtTargetWithEndstopRunsOpen) {
  this->cover_.set_has_built_in_endstop(true);
  this->cover_.position = COVER_OPEN;
  this->move_to_(COVER_OPEN);

  EXPECT_EQ(this->open_count_, 1);
  EXPECT_EQ(this->stop_count_, 0);
}

}  // namespace esphome::time_based::testing
