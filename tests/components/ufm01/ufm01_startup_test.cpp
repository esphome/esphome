#include "common.h"

#include "esphome/core/component.h"

namespace esphome::ufm01::testing {

TEST(UFM01SetupPriority, IsLate) {
  TestableUFM01 ufm01;
  EXPECT_EQ(ufm01.get_setup_priority(), setup_priority::LATE);
}

TEST_F(UFM01Test, ConsumeAckFindsByteAmongGarbage) {
  this->mock_uart_.enqueue({0x00, 0x01, COMMAND_ACK, 0x02});

  EXPECT_TRUE(this->ufm01_.consume_ack());
  EXPECT_EQ(this->mock_uart_.available(), 1u);
}

TEST_F(UFM01Test, ConsumeAckReturnsFalseWhenEmpty) { EXPECT_FALSE(this->ufm01_.consume_ack()); }

TEST_F(UFM01Test, StartupWaitDetectsActiveStream) {
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.init_wait_phase();

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::ACTIVE_STREAM);
  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::WAIT);
}

TEST_F(UFM01Test, StartPassiveReadSendsCommand) {
  this->ufm01_.start_passive_read();

  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[0], 0xFE);
  EXPECT_EQ(this->mock_uart_.written_data[1], 0xFE);
  EXPECT_EQ(this->mock_uart_.written_data[2], 0x11);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5B);
  EXPECT_EQ(this->mock_uart_.written_data[6], FRAME_STOP_BYTE);
}

TEST_F(UFM01Test, StaleActiveStreamSendsSetPassiveMode) {
  // Leftover stream noise should be flushed before SET_PASSIVE_MODE
  this->mock_uart_.enqueue({0x3C, 0x32, 0x00});
  this->ufm01_.prepare_stale_active_stream();

  this->ufm01_.loop_active_stream();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::ENTERING_PASSIVE);
  EXPECT_EQ(this->ufm01_.read_index(), 0);
  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[0], 0xFE);
  EXPECT_EQ(this->mock_uart_.written_data[1], 0xFE);
  EXPECT_EQ(this->mock_uart_.written_data[2], 0x11);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5C);  // set mode
  EXPECT_EQ(this->mock_uart_.written_data[4], 0x01);  // passive
  EXPECT_EQ(this->mock_uart_.written_data[5], 0x5D);  // checksum
  EXPECT_EQ(this->mock_uart_.written_data[6], FRAME_STOP_BYTE);
  EXPECT_EQ(this->mock_uart_.available(), 0u);  // RX flushed
}

TEST_F(UFM01Test, EnteringPassiveAdvancesOnAck) {
  this->ufm01_.set_operating_mode(OperatingMode::ENTERING_PASSIVE);
  this->mock_uart_.enqueue({0x00, COMMAND_ACK});

  this->ufm01_.loop_entering_passive();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL);
}

TEST_F(UFM01Test, EnteringPassiveStaysPendingWithoutAck) {
  this->ufm01_.prepare_stale_active_stream();
  this->ufm01_.loop_active_stream();  // ENTERING_PASSIVE with fresh phase_start_ms_
  ASSERT_EQ(this->ufm01_.operating_mode(), OperatingMode::ENTERING_PASSIVE);

  this->ufm01_.loop_entering_passive();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::ENTERING_PASSIVE);
}

TEST_F(UFM01Test, PassivePollSuccessClearsFailureCount) {
  this->ufm01_.set_operating_mode(OperatingMode::PASSIVE_POLL);

  for (int i = 0; i < 3; ++i) {
    this->ufm01_.prepare_timed_out_passive_read();
    this->ufm01_.loop_passive_poll();
  }
  ASSERT_EQ(this->ufm01_.consecutive_passive_failures(), 3u);

  auto frame = make_passive_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.begin_pending_passive_read();
  this->ufm01_.loop_passive_poll();

  EXPECT_EQ(this->ufm01_.consecutive_passive_failures(), 0u);
  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL);
}

TEST_F(UFM01Test, RepeatedPassiveFailuresEscalateToReset) {
  // Matches PASSIVE_FAIL_ESCALATE_COUNT in ufm01.cpp
  constexpr uint8_t escalate_count = 8;
  this->ufm01_.set_operating_mode(OperatingMode::PASSIVE_POLL);

  for (uint8_t i = 0; i < escalate_count - 1; ++i) {
    this->ufm01_.prepare_timed_out_passive_read();
    this->ufm01_.loop_passive_poll();
    ASSERT_EQ(this->ufm01_.operating_mode(), OperatingMode::PASSIVE_POLL) << "i=" << static_cast<int>(i);
  }
  EXPECT_EQ(this->ufm01_.consecutive_passive_failures(), escalate_count - 1);

  this->mock_uart_.written_data.clear();
  this->ufm01_.prepare_timed_out_passive_read();
  this->ufm01_.loop_passive_poll();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::STARTUP);
  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::RESET_WAIT_ACK);
  EXPECT_EQ(this->ufm01_.consecutive_passive_failures(), 0u);
  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5D);  // RESET_DEVICE
  EXPECT_EQ(this->mock_uart_.written_data[4], 0xCB);
}

}  // namespace esphome::ufm01::testing
