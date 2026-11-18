#include "common.h"

namespace esphome::ufm01::testing {

TEST_F(UFM01Test, ClearAccumulatedFlowSendsCommand) {
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow();

  EXPECT_TRUE(this->mock_uart_.written_data.empty());
  this->ufm01_.loop_pending_clear_action();

  this->expect_written(CLEAR_ACCUMULATED_FLOW);
  EXPECT_TRUE(this->ufm01_.clear_pending());
}

TEST_F(UFM01Test, ClearAccumulatedFlowFinishesOnAck) {
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow();
  this->ufm01_.loop_pending_clear_action();
  this->mock_uart_.enqueue({COMMAND_ACK});

  this->ufm01_.loop_pending_clear_action();

  EXPECT_FALSE(this->ufm01_.clear_pending());
}

TEST_F(UFM01Test, ClearAccumulatedFlowFinishesOnTimeout) {
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow();
  this->ufm01_.loop_pending_clear_action();
  this->advance(400);
  this->ufm01_.loop_pending_clear_action();
  ASSERT_TRUE(this->ufm01_.clear_pending());

  this->advance(200);
  this->ufm01_.loop_pending_clear_action();

  EXPECT_FALSE(this->ufm01_.clear_pending());
}

TEST_F(UFM01Test, DuplicateClearRequestJoinsThePendingOne) {
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.request_clear_accumulated_flow();
  this->ufm01_.loop_pending_clear_action();
  this->mock_uart_.written_data.clear();

  this->ufm01_.request_clear_accumulated_flow();
  this->ufm01_.loop_pending_clear_action();
  EXPECT_TRUE(this->mock_uart_.written_data.empty());

  this->mock_uart_.enqueue({COMMAND_ACK});
  this->ufm01_.loop_pending_clear_action();
  EXPECT_FALSE(this->ufm01_.clear_pending());

  this->ufm01_.request_clear_accumulated_flow();
  this->ufm01_.loop_pending_clear_action();
  this->expect_written(CLEAR_ACCUMULATED_FLOW);
}

TEST_F(UFM01Test, ClearWaitsForPendingPassiveRead) {
  this->ufm01_.set_operating_mode(OperatingMode::PASSIVE_POLL);
  this->ufm01_.begin_pending_passive_read();
  this->ufm01_.request_clear_accumulated_flow();

  this->ufm01_.loop_pending_clear_action();
  EXPECT_TRUE(this->mock_uart_.written_data.empty());

  this->mock_uart_.enqueue(make_passive_frame());
  this->ufm01_.loop_passive_poll();
  this->ufm01_.loop_pending_clear_action();

  this->expect_written(CLEAR_ACCUMULATED_FLOW);
}

TEST_F(UFM01Test, ClearWaitsForPendingSoftwareVersionRead) {
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.start_software_version_read();
  this->mock_uart_.written_data.clear();
  this->ufm01_.request_clear_accumulated_flow();

  this->ufm01_.loop_pending_clear_action();
  EXPECT_TRUE(this->mock_uart_.written_data.empty());

  this->mock_uart_.enqueue(make_software_version_response());
  this->ufm01_.loop_active_stream();
  this->ufm01_.loop_pending_clear_action();

  this->expect_written(CLEAR_ACCUMULATED_FLOW);
}

TEST_F(UFM01Test, ClearWaitsDuringStartup) {
  this->ufm01_.set_operating_mode(OperatingMode::STARTUP);
  this->ufm01_.request_clear_accumulated_flow();

  this->ufm01_.loop_pending_clear_action();

  EXPECT_TRUE(this->mock_uart_.written_data.empty());
  EXPECT_TRUE(this->ufm01_.clear_pending());
  EXPECT_FALSE(this->ufm01_.pending_clear_sent());
}

TEST_F(UFM01Test, ClearTimesOutWhileWaitingForIdle) {
  this->ufm01_.set_operating_mode(OperatingMode::STARTUP);
  this->ufm01_.request_clear_accumulated_flow();
  this->advance(20000);

  this->ufm01_.loop_pending_clear_action();

  EXPECT_FALSE(this->ufm01_.clear_pending());
  EXPECT_TRUE(this->mock_uart_.written_data.empty());
}

}  // namespace esphome::ufm01::testing
