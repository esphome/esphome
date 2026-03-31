#include "common.h"

using namespace esphome::valve;

namespace esphome::time_based::testing {

class TimeBasedValvePositionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    valve.mock_millis = 1000;
    valve.set_duration(10000);
    valve.setup();
  }

  TestableTimeBasedValve valve;
};

TEST_F(TimeBasedValvePositionTest, TriggersAreCalled) {
  auto stop_action = MockAction();
  auto stop_automation = Automation<>(valve.get_stop_trigger());
  stop_automation.add_action(&stop_action);
  auto open_action = MockAction();
  auto open_automation = Automation<>(valve.get_open_trigger());
  open_automation.add_action(&open_action);
  auto close_action = MockAction();
  auto close_automation = Automation<>(valve.get_close_trigger());
  close_automation.add_action(&close_action);

  EXPECT_CALL(stop_action, play()).Times(0);
  EXPECT_CALL(open_action, play()).Times(1);
  EXPECT_CALL(close_action, play()).Times(0);
  auto call = valve.make_call();
  call.set_command_open();
  call.perform();
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_OPENING);

  // Direction change
  ::testing::Mock::VerifyAndClearExpectations(&stop_action);
  ::testing::Mock::VerifyAndClearExpectations(&open_action);
  ::testing::Mock::VerifyAndClearExpectations(&close_action);
  EXPECT_CALL(stop_action, play()).Times(1);
  EXPECT_CALL(open_action, play()).Times(0);
  EXPECT_CALL(close_action, play()).Times(1);
  valve.mock_millis += 1000;
  call = valve.make_call();
  call.set_command_close();
  call.perform();
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);

  ::testing::Mock::VerifyAndClearExpectations(&stop_action);
  ::testing::Mock::VerifyAndClearExpectations(&open_action);
  ::testing::Mock::VerifyAndClearExpectations(&close_action);
  EXPECT_CALL(stop_action, play()).Times(1);
  EXPECT_CALL(open_action, play()).Times(0);
  EXPECT_CALL(close_action, play()).Times(0);
  valve.mock_millis += 1000;
  call = valve.make_call();
  call.set_command_stop();
  call.perform();
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);
}

TEST_F(TimeBasedValvePositionTest, RestoredPositionOpen) {
  valve.position = 0.5f;

  // Should be open, but full duration not traveled yet
  valve.set_position(0.6, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_OPENING);
  valve.mock_millis += 6000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, 0.6f);
  EXPECT_EQ(valve.position, 0.99f);
  EXPECT_FALSE(valve.is_fully_open());
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Full duration now traveled
  valve.set_position(0.5, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_OPENING);
  valve.mock_millis += 5000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, 1.0f);
  EXPECT_EQ(valve.position, VALVE_OPEN);
  EXPECT_TRUE(valve.is_fully_open());
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Further open is ignored
  valve.set_position(0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);
  valve.mock_millis += 1000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, 1.0f);
  EXPECT_EQ(valve.position, VALVE_OPEN);
}

TEST_F(TimeBasedValvePositionTest, RestoredPositionClose) {
  valve.position = 0.5f;

  // Open a little
  valve.set_position(0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_OPENING);
  valve.mock_millis += 1000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, 0.1f);
  EXPECT_EQ(valve.position, 0.6f);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Almost close
  valve.set_position(-0.6f, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 6000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -0.5f);
  EXPECT_EQ(valve.position, 0.01f);
  EXPECT_FALSE(valve.is_fully_closed());
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Close fully
  valve.set_position(-0.4, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 4000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -0.9f);
  EXPECT_EQ(valve.position, VALVE_CLOSED);
  EXPECT_TRUE(valve.is_fully_closed());
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Further close is ignored
  valve.set_position(-0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);
  valve.mock_millis += 1000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -0.9f);
  EXPECT_EQ(valve.position, VALVE_CLOSED);
}

TEST_F(TimeBasedValvePositionTest, SetPosition) {
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  valve.set_position(-0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 1000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -0.1f);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Position should still be NaN because not entire duration traveled
  valve.set_position(-0.4, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 4000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -0.5f);
  EXPECT_TRUE(std::isnan(valve.position));
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Move -0.1 over endstop to check clamping and position now available
  valve.set_position(-0.6, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 6000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -1.0f);
  EXPECT_EQ(valve.position, VALVE_CLOSED);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Check if not triggered at endstop
  valve.set_position(-0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);
  valve.mock_millis += 1000;
  valve.loop();
  EXPECT_EQ(valve.last_recompute_time_, 12000);

  // Move to other end
  valve.set_position(1, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_OPENING);
  valve.mock_millis += 10000;
  valve.loop();
  EXPECT_EQ(valve.position, VALVE_OPEN);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Move to middle absolute
  valve.set_position(0.5);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 5000;
  valve.loop();
  EXPECT_EQ(valve.position, 0.5f);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Open and close a little relative
  valve.set_position(0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_OPENING);
  valve.mock_millis += 1000;
  valve.loop();
  valve.set_position(-0.3, true);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 3000;
  valve.loop();
  EXPECT_EQ(valve.position, 0.3f);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);

  // Fully close
  valve.set_position(0);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_CLOSING);
  valve.mock_millis += 3000;
  valve.loop();
  EXPECT_EQ(valve.position, VALVE_CLOSED);
  EXPECT_EQ((int) valve.current_operation, (int) VALVE_OPERATION_IDLE);
}

}  // namespace esphome::time_based::testing
