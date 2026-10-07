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

// --- Garbage then valid frame tests ---

TEST_F(LD2450ReadlineTest, GarbageThenValidFrame) {
  // Garbage bytes accumulate in the buffer but don't match any footer.
  // A valid frame follows; its footer resets the buffer and resyncs.
  std::vector<uint8_t> garbage = {0x01, 0x02, 0x03, 0x42, 0x99};
  this->ld2450_.feed(garbage);
  EXPECT_GT(this->ld2450_.buffer_pos_, 0);  // Garbage accumulated

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  // Footer from the valid frame resyncs the parser
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

// --- Footer-based resynchronization tests ---

TEST_F(LD2450ReadlineTest, FooterInGarbageResyncs) {
  // Garbage containing a periodic frame footer (0x55 0xCC) triggers
  // a buffer reset, allowing the next frame to be parsed cleanly.
  std::vector<uint8_t> garbage_with_footer = {0x01, 0x02, 0x03, 0x04, 0x55, 0xCC};
  this->ld2450_.feed(garbage_with_footer);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);  // Footer reset the buffer

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, CmdFooterInGarbageResyncs) {
  // Garbage containing a command frame footer (04 03 02 01) also resyncs.
  std::vector<uint8_t> garbage_with_footer = {0x10, 0x20, 0x30, 0x40, 0x04, 0x03, 0x02, 0x01};
  this->ld2450_.feed(garbage_with_footer);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

// --- Overflow recovery tests ---

TEST_F(LD2450ReadlineTest, OverflowResetsBuffer) {
  // Fill the buffer to capacity with filler that won't match any footer.
  // MAX_LINE_LENGTH is 45, usable is 44. The 45th byte triggers overflow.
  std::vector<uint8_t> overflow_data(MAX_LINE_LENGTH, 0x11);
  this->ld2450_.feed(overflow_data);
  // After overflow, buffer_pos_ resets to 0 (via the < 4 early return path)
  EXPECT_LT(this->ld2450_.buffer_pos_, 4);
}

TEST_F(LD2450ReadlineTest, OverflowThenValidFrame) {
  // Overflow, then a valid frame should be processed.
  std::vector<uint8_t> overflow_data(MAX_LINE_LENGTH, 0x11);
  this->ld2450_.feed(overflow_data);

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, BufferLargeEnoughForDesyncedFooter) {
  // The key fix: the buffer (45) is large enough that a desynced periodic frame's
  // footer (at most 30 bytes into the stream) will land inside the buffer before overflow.
  // Simulate starting 10 bytes into a periodic frame, then a full frame follows.
  std::vector<uint8_t> mid_frame = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
  // Then a complete periodic frame whose footer will land at position 40 (10 + 30),
  // well within the buffer size of 45.
  auto frame = make_periodic_frame();
  mid_frame.insert(mid_frame.end(), frame.begin(), frame.end());

  this->ld2450_.feed(mid_frame);
  // The footer from the frame should have triggered a reset
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, SimulatedRestartThenFrames) {
  // Simulate LD2450 restart: burst of garbage followed by valid periodic frames.
  // The garbage + first frame should fit in the buffer so the footer resyncs.
  std::vector<uint8_t> restart_noise = {
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // 8 bytes of mid-frame data
  };
  auto frame = make_periodic_frame();
  // 8 garbage + 30 frame = 38 bytes, well within buffer of 45
  restart_noise.insert(restart_noise.end(), frame.begin(), frame.end());

  this->ld2450_.feed(restart_noise);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  // Subsequent frames should work normally
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

}  // namespace esphome::ld2450::testing
