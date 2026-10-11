#include "common.h"

namespace esphome::ufm01::testing {

TEST_F(UFM01Test, ValidActiveFrameAccepted) {
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));

  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_EQ(this->ufm01_.read_index(), 0);
  EXPECT_NE(this->ufm01_.last_valid_frame_ms(), 0u);
}

TEST_F(UFM01Test, GarbagePrefixThenValidActiveFrame) {
  this->mock_uart_.enqueue({0x00, 0xFF, 0xAA});
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));

  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_EQ(this->ufm01_.read_index(), 0);
}

TEST_F(UFM01Test, InvalidActiveFrameChecksumRejected) {
  auto frame = make_active_frame();
  frame[30] ^= 0xFF;
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));

  EXPECT_FALSE(this->ufm01_.process_active_stream());
  EXPECT_EQ(this->ufm01_.read_index(), 0);
  EXPECT_EQ(this->ufm01_.last_valid_frame_ms(), 0u);
}

TEST_F(UFM01Test, ValidPassiveFrameReadSuccess) {
  auto frame = make_passive_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.prepare_passive_read();

  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS);
  EXPECT_EQ(this->ufm01_.passive_index(), PASSIVE_FRAME_SIZE);
  EXPECT_NE(this->ufm01_.last_valid_frame_ms(), 0u);
}

TEST_F(UFM01Test, InvalidPassiveChecksumFails) {
  auto frame = make_passive_frame();
  frame[21] ^= 0xFF;
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.prepare_passive_read();

  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_FAILURE);
  EXPECT_EQ(this->ufm01_.last_valid_frame_ms(), 0u);
}

TEST_F(UFM01Test, PassiveReadResyncsAfterGarbagePrefix) {
  auto frame = make_passive_frame();
  this->mock_uart_.enqueue({0x00, 0x01, 0x02});
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.prepare_passive_read();

  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS);
}

TEST_F(UFM01Test, PassiveReadResyncsOnSecondStartByte) {
  auto frame = make_passive_frame();
  this->mock_uart_.enqueue({FRAME_START_BYTE_1, 0x99});
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.prepare_passive_read();

  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS);
}

TEST_F(UFM01Test, PassiveReadPendingWhenPartial) {
  auto frame = make_passive_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.begin() + 10));
  this->ufm01_.prepare_passive_read();

  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_PENDING);
  EXPECT_LT(this->ufm01_.passive_index(), PASSIVE_FRAME_SIZE);

  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin() + 10, frame.end()));
  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS);
}

}  // namespace esphome::ufm01::testing
