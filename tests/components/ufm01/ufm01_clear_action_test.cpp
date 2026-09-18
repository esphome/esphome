#include "common.h"

namespace esphome::ufm01::testing {

#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION

class MockClearAction : public ClearAccumulatedFlowActionInterface {
 public:
  void complete() override { this->completed = true; }

  bool completed{false};
};

TEST_F(UFM01Test, ClearAccumulatedFlowSendsCommand) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow(&action);

  EXPECT_TRUE(this->mock_uart_.written_data.empty());
  this->ufm01_.loop_pending_clear_action();

  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5A);
  EXPECT_EQ(this->mock_uart_.written_data[4], 0xFD);
  EXPECT_FALSE(action.completed);
}

TEST_F(UFM01Test, ClearAccumulatedFlowCompletesOnAck) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow(&action);
  this->ufm01_.loop_pending_clear_action();
  this->mock_uart_.enqueue({COMMAND_ACK});

  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(action.completed);
}

TEST_F(UFM01Test, ClearAccumulatedFlowCompletesOnTimeout) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow(&action);
  this->ufm01_.loop_pending_clear_action();
  this->ufm01_.set_pending_clear_start_ms(millis() - 2000);

  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(action.completed);
}

TEST_F(UFM01Test, DuplicateClearRequestIsIgnored) {
  MockClearAction first;
  MockClearAction second;
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow(&first);

  this->ufm01_.request_clear_accumulated_flow(&second);

  EXPECT_FALSE(first.completed);
  EXPECT_FALSE(second.completed);
}

TEST_F(UFM01Test, ClearWaitsForPendingPassiveRead) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::PASSIVE_POLL);
  this->ufm01_.begin_pending_passive_read();
  this->ufm01_.request_clear_accumulated_flow(&action);

  this->ufm01_.loop_pending_clear_action();
  EXPECT_TRUE(this->mock_uart_.written_data.empty());

  auto frame = make_passive_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.loop_passive_poll();
  this->ufm01_.loop_pending_clear_action();

  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5A);
  EXPECT_FALSE(action.completed);
}

TEST_F(UFM01Test, ClearWaitsDuringStartup) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::STARTUP);
  this->ufm01_.request_clear_accumulated_flow(&action);

  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(this->mock_uart_.written_data.empty());
  EXPECT_FALSE(action.completed);
  EXPECT_FALSE(this->ufm01_.pending_clear_sent());
}

TEST_F(UFM01Test, ClearTimesOutWhileWaitingForIdle) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::ENTERING_PASSIVE);
  this->ufm01_.request_clear_accumulated_flow(&action);
  this->ufm01_.set_pending_clear_start_ms(millis() - 20000);

  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(action.completed);
  EXPECT_TRUE(this->mock_uart_.written_data.empty());
}

TEST_F(UFM01Test, CancelPendingClearDoesNotSend) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow(&action);

  this->ufm01_.cancel_pending_clear_action(&action);
  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(this->mock_uart_.written_data.empty());
  EXPECT_FALSE(action.completed);
}

#endif  // USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION

}  // namespace esphome::ufm01::testing
