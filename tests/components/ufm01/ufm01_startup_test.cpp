#include "common.h"

namespace esphome::ufm01::testing {

TEST_F(UFM01Test, ConsumeAckFindsByteAmongGarbage) {
  this->mock_uart_.enqueue({0x00, 0x01, COMMAND_ACK, 0x02});

  EXPECT_TRUE(this->ufm01_.consume_ack());
  EXPECT_EQ(this->mock_uart_.available(), 1u);
}

TEST_F(UFM01Test, ConsumeAckReturnsFalseWhenEmpty) { EXPECT_FALSE(this->ufm01_.consume_ack()); }

TEST_F(UFM01Test, StartupWaitDetectsActiveStream) {
  this->mock_uart_.enqueue(make_active_frame());
  this->ufm01_.init_wait_phase();

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::ACTIVE_STREAM);
  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::WAIT);
}

TEST_F(UFM01Test, StartPassiveReadSendsCommand) {
  this->ufm01_.start_passive_read();

  this->expect_written(READ_SENSOR_DATA_NO_ID);
}

TEST_F(UFM01Test, StartupActiveWaitTimeoutSendsSetPassiveMode) {
  this->ufm01_.begin_active_wait_frame();
  this->advance(4000);

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::SET_PASSIVE_WAIT_ACK);
  this->expect_written(PASSIVE_MODE);
}

TEST_F(UFM01Test, StartupSetPassiveWaitAckStartsPassiveRead) {
  this->ufm01_.begin_active_wait_frame();
  this->advance(4000);
  this->ufm01_.loop_startup();
  this->mock_uart_.enqueue({COMMAND_ACK});
  this->mock_uart_.written_data.clear();

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::PASSIVE_WAIT_REPLY);
  this->expect_written(READ_SENSOR_DATA_NO_ID);
}

TEST_F(UFM01Test, StaleActiveStreamSendsSetPassiveMode) {
  // Leftover stream noise should be flushed before SET_PASSIVE_MODE
  this->mock_uart_.enqueue({0x3C, 0x32, 0x00});
  this->make_active_stream_stale();

  this->ufm01_.loop_active_stream();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL);
  EXPECT_EQ(this->ufm01_.read_index(), 0);
  this->expect_written(PASSIVE_MODE);
  EXPECT_EQ(this->mock_uart_.available(), 0u);  // RX flushed
}

TEST_F(UFM01Test, StalePassivePollWaitsOneIntervalAndDiscardsLateAck) {
  this->make_active_stream_stale();
  this->ufm01_.loop_active_stream();
  ASSERT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL);
  this->mock_uart_.written_data.clear();

  this->mock_uart_.enqueue({COMMAND_ACK});
  this->advance(500);
  this->ufm01_.loop_passive_poll();
  EXPECT_TRUE(this->mock_uart_.written_data.empty());
  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL);

  this->advance(500);
  this->ufm01_.loop_passive_poll();
  this->expect_written(READ_SENSOR_DATA_NO_ID);
  EXPECT_EQ(this->mock_uart_.available(), 0u);
}

#ifdef USE_SENSOR
TEST_F(UFM01Test, StaleActiveStreamPublishesNanToFlowAndTemperature) {
  this->attach_flow_and_temperature_sensors();
  this->make_active_stream_stale();

  this->ufm01_.loop_active_stream();

  EXPECT_TRUE(std::isnan(this->flow_sensor_.get_state()));
  EXPECT_TRUE(std::isnan(this->temperature_sensor_.get_state()));
}

TEST_F(UFM01Test, RepeatedPassiveFailuresPublishNanBeforeReset) {
  this->attach_flow_and_temperature_sensors();
  this->ufm01_.set_operating_mode(OperatingMode::PASSIVE_POLL);

  for (uint8_t i = 0; i < ESCALATE_COUNT; ++i)
    this->fail_passive_poll();

  EXPECT_TRUE(std::isnan(this->flow_sensor_.get_state()));
  EXPECT_TRUE(std::isnan(this->temperature_sensor_.get_state()));
}
#endif

TEST_F(UFM01Test, PassivePollSuccessClearsFailureCount) {
  this->ufm01_.set_operating_mode(OperatingMode::PASSIVE_POLL);

  for (int i = 0; i < 3; ++i)
    this->fail_passive_poll();
  ASSERT_EQ(this->ufm01_.consecutive_passive_failures(), 3u);

  this->mock_uart_.enqueue(make_passive_frame());
  this->ufm01_.begin_pending_passive_read();
  this->ufm01_.loop_passive_poll();

  EXPECT_EQ(this->ufm01_.consecutive_passive_failures(), 0u);
  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL);
}

TEST_F(UFM01Test, RepeatedPassiveFailuresEscalateToReset) {
  this->ufm01_.set_operating_mode(OperatingMode::PASSIVE_POLL);

  for (uint8_t i = 0; i < ESCALATE_COUNT - 1; ++i) {
    this->fail_passive_poll();
    ASSERT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL) << "i=" << static_cast<int>(i);
  }
  EXPECT_EQ(this->ufm01_.consecutive_passive_failures(), ESCALATE_COUNT - 1);

  this->mock_uart_.written_data.clear();
  this->fail_passive_poll();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::STARTUP);
  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::RESET_WAIT_ACK);
  EXPECT_EQ(this->ufm01_.consecutive_passive_failures(), 0u);
  this->expect_written(RESET_DEVICE);
}

}  // namespace esphome::ufm01::testing
