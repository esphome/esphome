#include "common.h"

namespace esphome::ufm01::testing {

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

  this->expect_written(CLEAR_ACCUMULATED_FLOW);
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
  this->advance(400);
  this->ufm01_.loop_pending_clear_action();
  ASSERT_FALSE(action.completed);

  this->advance(200);
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

  this->mock_uart_.enqueue(make_passive_frame());
  this->ufm01_.loop_passive_poll();
  this->ufm01_.loop_pending_clear_action();

  this->expect_written(CLEAR_ACCUMULATED_FLOW);
  EXPECT_FALSE(action.completed);
}

TEST_F(UFM01Test, ClearWaitsForPendingSoftwareVersionRead) {
  MockClearAction action;
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.start_software_version_read();
  this->mock_uart_.written_data.clear();
  this->ufm01_.request_clear_accumulated_flow(&action);

  this->ufm01_.loop_pending_clear_action();
  EXPECT_TRUE(this->mock_uart_.written_data.empty());

  this->mock_uart_.enqueue(make_software_version_response());
  this->ufm01_.loop_active_stream();
  this->ufm01_.loop_pending_clear_action();

  this->expect_written(CLEAR_ACCUMULATED_FLOW);
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
  this->ufm01_.set_operating_mode(OperatingMode::STARTUP);
  this->ufm01_.request_clear_accumulated_flow(&action);
  this->advance(20000);

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

}  // namespace esphome::ufm01::testing
