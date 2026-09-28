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

}  // namespace esphome::ufm01::testing
