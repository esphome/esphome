#include "common.h"

using namespace esphome::valve;

namespace esphome::time_based::testing {

class TimeBasedValvePositionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    valve.set_duration(10000);
    valve.setup();
  }

  TestableTimeBasedValve valve;
};

TEST_F(TimeBasedValvePositionTest, SetPosition) {
  valve.mock_millis = 1000;
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);

  valve.set_position(-0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_CLOSING);
  valve.mock_millis = 2000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -0.1f);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);

  // Position should still be null because not entire duration traveled
  valve.set_position(-0.4, true);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_CLOSING);
  valve.mock_millis = 6000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -0.5f);
  EXPECT_TRUE(std::isnan(valve.position));
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);

  // Move -0.1 over endstop to check clamping and position now available
  valve.set_position(-0.6, true);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_CLOSING);
  valve.mock_millis = 12000;
  valve.loop();
  EXPECT_EQ(valve.measured_position_, -1.0f);
  EXPECT_EQ(valve.position, 0.0f);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);

  // Check if not triggered at endstop
  valve.set_position(-0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);
  valve.mock_millis = 13000;
  valve.loop();
  EXPECT_EQ(valve.last_recompute_time_, 12000);

  // Move to other end
  valve.set_position(1, true);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_OPENING);
  valve.mock_millis = 23000;
  valve.loop();
  EXPECT_EQ(valve.position, VALVE_OPEN);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);

  // Move to middle absolute
  valve.set_position(0.5);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_CLOSING);
  valve.mock_millis = 28000;
  valve.loop();
  EXPECT_EQ(valve.position, 0.5f);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);

  // Open and close a little relative
  valve.set_position(0.1, true);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_OPENING);
  valve.mock_millis = 29000;
  valve.loop();
  valve.set_position(-0.3, true);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_CLOSING);
  valve.mock_millis = 32000;
  valve.loop();
  EXPECT_EQ(valve.position, 0.3f);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);

  // Fully close
  valve.set_position(0);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_CLOSING);
  valve.mock_millis = 35000;
  valve.loop();
  EXPECT_EQ(valve.position, VALVE_CLOSED);
  EXPECT_EQ((int) valve.current_operation, (int) ValveOperation::VALVE_OPERATION_IDLE);
}

}  // namespace esphome::time_based::testing
