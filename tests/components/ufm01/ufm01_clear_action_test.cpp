#include "common.h"

#include "esphome/components/ufm01/automation.h"

namespace esphome::ufm01::testing {

class MockClearAction : public ClearAccumulatedFlowActionInterface {
 public:
  void complete() override { this->completed = true; }

  bool completed{false};
};

TEST_F(UFM01Test, ClearAccumulatedFlowSendsCommand) {
  MockClearAction action;
  this->ufm01_.request_clear_accumulated_flow(&action);

  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5A);
  EXPECT_EQ(this->mock_uart_.written_data[4], 0xFD);
  EXPECT_FALSE(action.completed);
}

TEST_F(UFM01Test, ClearAccumulatedFlowCompletesOnAck) {
  MockClearAction action;
  this->ufm01_.request_clear_accumulated_flow(&action);
  this->mock_uart_.enqueue({COMMAND_ACK});

  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(action.completed);
}

TEST_F(UFM01Test, ClearAccumulatedFlowCompletesOnTimeout) {
  MockClearAction action;
  this->ufm01_.request_clear_accumulated_flow(&action);
  this->ufm01_.set_pending_clear_start_ms(millis() - 2000);

  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(action.completed);
}

TEST_F(UFM01Test, DuplicateClearRequestCompletesImmediately) {
  MockClearAction first;
  MockClearAction second;
  this->ufm01_.request_clear_accumulated_flow(&first);

  this->ufm01_.request_clear_accumulated_flow(&second);

  EXPECT_FALSE(first.completed);
  EXPECT_TRUE(second.completed);
}

}  // namespace esphome::ufm01::testing
