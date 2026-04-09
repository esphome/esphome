#include "common.h"

using namespace esphome::valve;

namespace esphome::time_based::testing {

class TimeBasedValvePositionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_EQ(valve_.position, 0.5f);
    valve_.mock_millis = 1000;
    valve_.set_duration(10000);
    valve_.setup();
  }

  TestableTimeBasedValve valve_;
};

TEST_F(TimeBasedValvePositionTest, TriggersAreCalled) {
  auto stop_action = MockAction();
  auto stop_automation = Automation<>(valve_.get_stop_trigger());
  stop_automation.add_action(&stop_action);
  auto open_action = MockAction();
  auto open_automation = Automation<>(valve_.get_open_trigger());
  open_automation.add_action(&open_action);
  auto close_action = MockAction();
  auto close_automation = Automation<>(valve_.get_close_trigger());
  close_automation.add_action(&close_action);

  EXPECT_CALL(stop_action, play()).Times(0);
  EXPECT_CALL(open_action, play()).Times(1);
  EXPECT_CALL(close_action, play()).Times(0);
  auto call = valve_.make_call();
  call.set_command_open();
  call.perform();
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);

  // Direction change
  ::testing::Mock::VerifyAndClearExpectations(&stop_action);
  ::testing::Mock::VerifyAndClearExpectations(&open_action);
  ::testing::Mock::VerifyAndClearExpectations(&close_action);
  EXPECT_CALL(stop_action, play()).Times(1);
  EXPECT_CALL(open_action, play()).Times(0);
  EXPECT_CALL(close_action, play()).Times(1);
  valve_.mock_millis += 1000;
  call = valve_.make_call();
  call.set_command_close();
  call.perform();
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);

  ::testing::Mock::VerifyAndClearExpectations(&stop_action);
  ::testing::Mock::VerifyAndClearExpectations(&open_action);
  ::testing::Mock::VerifyAndClearExpectations(&close_action);
  EXPECT_CALL(stop_action, play()).Times(1);
  EXPECT_CALL(open_action, play()).Times(0);
  EXPECT_CALL(close_action, play()).Times(0);
  valve_.mock_millis += 1000;
  call = valve_.make_call();
  call.set_command_stop();
  call.perform();
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);
}

TEST_F(TimeBasedValvePositionTest, RestoredPositionFullyClosed) {
  valve_.position = 0;
  valve_.set_restore_mode(VALVE_RESTORE);
  valve_.setup();
  EXPECT_EQ(valve_.position, 0.01f);
}

TEST_F(TimeBasedValvePositionTest, OpenWhileOpening) {
  valve_.set_position(0.4, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);
  valve_.mock_millis += 2000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, 0.2f);

  // Open while still opening
  valve_.set_position(0.1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);
  valve_.mock_millis += 3000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, 0.5f);

  // Should have stopped
  valve_.mock_millis += 1000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, 0.5f);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);
}

TEST_F(TimeBasedValvePositionTest, RestoredPositionOpen) {
  valve_.position = 0.7f;

  // Should be open, but full duration not traveled yet
  valve_.set_position(0.4, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);
  valve_.mock_millis += 4000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, 0.4f);
  EXPECT_EQ(valve_.position, 0.99f);
  EXPECT_FALSE(valve_.is_endstop_reached());
  EXPECT_FALSE(valve_.is_fully_open());
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Full duration now traveled
  valve_.set_position(0.7, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);
  valve_.mock_millis += 7000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, 1.0f);
  EXPECT_EQ(valve_.position, VALVE_OPEN);
  EXPECT_TRUE(valve_.is_endstop_reached());
  EXPECT_TRUE(valve_.is_fully_open());
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Further open is ignored
  valve_.set_position(0.1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);
  valve_.mock_millis += 1000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, 1.0f);
  EXPECT_EQ(valve_.position, VALVE_OPEN);
}

TEST_F(TimeBasedValvePositionTest, RestoredPositionClose) {
  valve_.position = 0.7f;

  // Open a little
  valve_.set_position(0.1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);
  valve_.mock_millis += 1000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, 0.1f);
  EXPECT_EQ(valve_.position, 0.8f);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Almost close
  valve_.set_position(-0.8f, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 8000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, -0.7f);
  EXPECT_EQ(valve_.position, 0.01f);
  EXPECT_FALSE(valve_.is_fully_closed());
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Close fully
  valve_.set_position(-0.2, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 2000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, -0.9f);
  EXPECT_EQ(valve_.position, VALVE_CLOSED);
  EXPECT_TRUE(valve_.is_fully_closed());
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Further close is ignored
  valve_.set_position(-0.1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);
  valve_.mock_millis += 1000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, -0.9f);
  EXPECT_EQ(valve_.position, VALVE_CLOSED);
}

TEST_F(TimeBasedValvePositionTest, SetPosition) {
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  valve_.set_position(-0.1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 1000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, -0.1f);
  EXPECT_EQ(valve_.position, 0.4f);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Position should be 0.01 because not entire duration traveled
  valve_.set_position(-0.4, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 4000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, -0.5f);
  EXPECT_EQ(valve_.position, 0.01f);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Move -0.1 over endstop to check clamping and position now available
  valve_.set_position(-0.6, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 6000;
  valve_.loop();
  EXPECT_EQ(valve_.measured_position_, -1.0f);
  EXPECT_EQ(valve_.position, VALVE_CLOSED);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Check if not triggered at endstop
  valve_.set_position(-0.1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);
  valve_.mock_millis += 1000;
  valve_.loop();
  EXPECT_EQ(valve_.last_recompute_time_, 12000);

  // Move to other end
  valve_.set_position(1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);
  valve_.mock_millis += 10000;
  valve_.loop();
  EXPECT_EQ(valve_.position, VALVE_OPEN);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Move to middle absolute
  valve_.set_position(0.5);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 5000;
  valve_.loop();
  EXPECT_EQ(valve_.position, 0.5f);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Open and close a little relative
  valve_.set_position(0.1, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_OPENING);
  valve_.mock_millis += 1000;
  valve_.loop();
  valve_.set_position(-0.3, true);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 3000;
  valve_.loop();
  EXPECT_EQ(valve_.position, 0.3f);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);

  // Fully close
  valve_.set_position(0);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve_.mock_millis += 3000;
  valve_.loop();
  EXPECT_EQ(valve_.position, VALVE_CLOSED);
  EXPECT_EQ((int) valve_.current_operation, (int) VALVE_OPERATION_IDLE);
}

}  // namespace esphome::time_based::testing
