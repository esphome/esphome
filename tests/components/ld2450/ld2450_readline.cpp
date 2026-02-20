#include "common.h"

namespace esphome::ld2450::testing {

class LD2450ReadlineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2450_.set_uart_parent(&this->mock_uart_);
    // Ensure clean state
    ASSERT_EQ(this->ld2450_.buffer_pos_, 0);
  }

  MockUARTComponent mock_uart_;
  TestableLD2450 ld2450_;
};

// --- Good data tests ---

TEST_F(LD2450ReadlineTest, ValidPeriodicFrame) {
  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  // After a complete valid frame, buffer should be reset
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, ValidCommandAckFrame) {
  auto frame = make_ack_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, BackToBackPeriodicFrames) {
  auto frame = make_periodic_frame();
  for (int i = 0; i < 5; i++) {
    this->ld2450_.feed(frame);
    EXPECT_EQ(this->ld2450_.buffer_pos_, 0) << "Frame " << i << " not processed";
  }
}

TEST_F(LD2450ReadlineTest, BackToBackMixedFrames) {
  auto periodic = make_periodic_frame();
  auto ack = make_ack_frame();
  this->ld2450_.feed(periodic);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  this->ld2450_.feed(ack);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  this->ld2450_.feed(periodic);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

// --- Garbage rejection tests ---

TEST_F(LD2450ReadlineTest, GarbageDiscarded) {
  // Feed bytes that don't match any header start byte
  std::vector<uint8_t> garbage = {0x01, 0x02, 0x03, 0x42, 0x99, 0x00, 0xFF, 0x7F};
  this->ld2450_.feed(garbage);
  // Header sync should discard all of these
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, GarbageThenValidFrame) {
  std::vector<uint8_t> garbage = {0x01, 0x02, 0x03, 0x42, 0x99};
  this->ld2450_.feed(garbage);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

// --- Header synchronization tests ---

TEST_F(LD2450ReadlineTest, PartialDataHeaderThenMismatch) {
  // Start of a data frame header, then invalid byte
  this->ld2450_.feed({0xAA, 0xFF, 0x42});  // 0x42 doesn't match DATA_FRAME_HEADER[2] (0x03)
  // Parser should have reset
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, PartialCmdHeaderThenMismatch) {
  // Start of a command frame header, then invalid byte
  this->ld2450_.feed({0xFD, 0xFC, 0xFB, 0x42});  // 0x42 doesn't match CMD_FRAME_HEADER[3] (0xFA)
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, PartialHeaderThenValidFrame) {
  // Partial header that fails, then a complete valid frame
  this->ld2450_.feed({0xAA, 0xFF, 0x42});  // Fails at byte 3
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, HeaderMismatchRecoveryOnNewHeaderByte) {
  // Start data header, mismatch at byte 2, but mismatch byte is start of command header
  this->ld2450_.feed({0xAA, 0xFF});
  EXPECT_EQ(this->ld2450_.buffer_pos_, 2);  // Accumulating header

  this->ld2450_.feed({0xFD});  // Doesn't match DATA_FRAME_HEADER[2]=0x03, but IS CMD_FRAME_HEADER[0]
  // Parser should reset and start new frame with 0xFD
  EXPECT_EQ(this->ld2450_.buffer_pos_, 1);
  EXPECT_EQ(this->ld2450_.buffer_data_[0], 0xFD);
}

// --- Mid-frame / overflow recovery tests ---

TEST_F(LD2450ReadlineTest, MidFrameDataRecovery) {
  // Simulate starting mid-frame: feed the tail end of a periodic frame (no valid header)
  // These bytes would be part of target data in a real frame
  std::vector<uint8_t> mid_frame = {0x10, 0x20, 0x30, 0x40, 0x55, 0xCC};
  this->ld2450_.feed(mid_frame);
  // All discarded (none match header start bytes)
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  // Now feed a valid frame
  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, OverflowRecovery) {
  // Feed a valid data frame header followed by enough filler to cause overflow.
  // Header (4) + 36 filler = 40 bytes in buffer. The 41st byte triggers overflow.
  std::vector<uint8_t> overflow_data = {0xAA, 0xFF, 0x03, 0x00};  // Valid header
  for (int i = 0; i < 37; i++) {
    overflow_data.push_back(0x11);  // Filler that won't match any footer
  }
  // 41 bytes total: 40 stored, 41st triggers overflow and resets buffer_pos_ to 0
  this->ld2450_.feed(overflow_data);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  // Feed a valid frame and verify recovery
  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, RepeatedOverflowDoesNotLoop) {
  // Simulate the bug scenario: repeated overflows should not prevent recovery.
  // Feed 3 rounds of overflow-inducing data.
  for (int round = 0; round < 3; round++) {
    std::vector<uint8_t> overflow_data = {0xAA, 0xFF, 0x03, 0x00};
    for (int i = 0; i < 37; i++) {
      overflow_data.push_back(0x22);
    }
    this->ld2450_.feed(overflow_data);
    EXPECT_EQ(this->ld2450_.buffer_pos_, 0) << "Overflow round " << round;
  }

  // Parser should still recover and process a valid frame
  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, SimulatedRestartGarbageThenFrames) {
  // Simulate LD2450 restart: burst of garbage bytes (partial frames, noise)
  // followed by normal periodic data.
  // Partial periodic frame (as if we started reading mid-frame), a stale footer, and more garbage
  std::vector<uint8_t> restart_noise = {
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,  // mid-frame data
      0x55, 0xCC,                                                                                // stale footer bytes
      0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,                                // more garbage
  };

  this->ld2450_.feed(restart_noise);
  // All garbage should be discarded
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  // Now the LD2450 starts sending valid frames
  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

}  // namespace esphome::ld2450::testing
