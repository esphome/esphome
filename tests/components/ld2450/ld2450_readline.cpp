#include "common.h"

namespace esphome::ld2450::testing {

class LD2450ReadlineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2450_.set_uart_parent(&this->mock_uart_);
    // Ensure clean state
    ASSERT_EQ(this->ld2450_.reader_.length(), 0u);
  }

  MockUARTComponent mock_uart_;
  TestableLD2450 ld2450_;
};

// --- Good data tests ---

TEST_F(LD2450ReadlineTest, ValidPeriodicFrame) {
  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  // After a complete valid frame, buffer should be reset
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

TEST_F(LD2450ReadlineTest, ValidCommandAckFrame) {
  auto frame = make_ack_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

TEST_F(LD2450ReadlineTest, BackToBackPeriodicFrames) {
  auto frame = make_periodic_frame();
  for (int i = 0; i < 5; i++) {
    this->ld2450_.feed(frame);
    EXPECT_EQ(this->ld2450_.reader_.length(), 0u) << "Frame " << i << " not processed";
  }
}

TEST_F(LD2450ReadlineTest, BackToBackMixedFrames) {
  auto periodic = make_periodic_frame();
  auto ack = make_ack_frame();
  this->ld2450_.feed(periodic);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
  this->ld2450_.feed(ack);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
  this->ld2450_.feed(periodic);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

// --- Noise then a frame ---

TEST_F(LD2450ReadlineTest, NoiseIsNotBufferedAndTheNextFrameParses) {
  // Plain bytes, footer bytes of both frame types, a run longer than the buffer, the tail of a frame
  const std::vector<std::vector<uint8_t>> noises = {
      {0x01, 0x02, 0x03, 0x42, 0x99},
      {0x01, 0x02, 0x03, 0x04, 0x55, 0xCC},
      {0x10, 0x20, 0x30, 0x40, 0x04, 0x03, 0x02, 0x01},
      std::vector<uint8_t>(MAX_LINE_LENGTH, 0x11),
      {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39},
  };
  for (const auto &noise : noises) {
    this->ld2450_.feed(noise);
    EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
    this->ld2450_.feed(make_periodic_frame());
    EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
  }
}

TEST_F(LD2450ReadlineTest, SimulatedRestartThenFrames) {
  // Simulate LD2450 restart: burst of garbage followed by valid periodic frames.
  std::vector<uint8_t> restart_noise = {
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // 8 bytes of mid-frame data
  };
  auto frame = make_periodic_frame();
  // 8 garbage + 30 frame = 38 bytes, well within buffer of 45
  restart_noise.insert(restart_noise.end(), frame.begin(), frame.end());

  this->ld2450_.feed(restart_noise);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);

  // Subsequent frames should work normally
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

// --- Frame payloads that look like footers ---

TEST_F(LD2450ReadlineTest, CmdFooterBytesInPeriodicPayloadDoNotEndTheFrame) {
  int frames = 0;
  this->ld2450_.add_on_data_callback([&frames]() { frames++; });
  // Target 1 at x 772 and y 258: its first four bytes are the command footer 04 03 02 01
  auto frame = make_periodic_frame();
  frame[4] = 0x04;
  frame[5] = 0x03;
  frame[6] = 0x02;
  frame[7] = 0x01;
  this->ld2450_.feed(frame);
  EXPECT_EQ(frames, 1);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);

  // The parser is still in step: the next frames are handled as usual
  this->ld2450_.feed(make_ack_frame());
  this->ld2450_.feed(make_periodic_frame());
  EXPECT_EQ(frames, 2);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

TEST_F(LD2450ReadlineTest, BogusLengthFieldResyncsOnTheNextHeader) {
  int frames = 0;
  this->ld2450_.add_on_data_callback([&frames]() { frames++; });
  // A command header claiming a 255 byte payload, then a complete periodic frame
  std::vector<uint8_t> stream = {0xFD, 0xFC, 0xFB, 0xFA, 0xFF, 0x00};
  auto frame = make_periodic_frame();
  stream.insert(stream.end(), frame.begin(), frame.end());
  this->ld2450_.feed(stream);
  EXPECT_EQ(frames, 1);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

TEST_F(LD2450ReadlineTest, LargestLengthFieldDoesNotWrapToIncomplete) {
  int frames = 0;
  this->ld2450_.add_on_data_callback([&frames]() { frames++; });
  // A 0xFFF6 byte payload makes the frame 65536 bytes long, which must not wrap around to 0
  std::vector<uint8_t> stream = {0xFD, 0xFC, 0xFB, 0xFA, 0xF6, 0xFF};
  stream.insert(stream.end(), MAX_LINE_LENGTH, 0x11);
  auto frame = make_periodic_frame();
  stream.insert(stream.end(), frame.begin(), frame.end());
  this->ld2450_.feed(stream);
  EXPECT_EQ(frames, 1);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

TEST_F(LD2450ReadlineTest, MixedHeaderPrefixNeverOpensAFrame) {
  int frames = 0;
  this->ld2450_.add_on_data_callback([&frames]() { frames++; });
  // A stray command header byte followed by a periodic header that lost its first byte: the stray byte
  // picks the command header, so the periodic bytes do not continue it and the buffer restarts on AA
  std::vector<uint8_t> stream = {0xFD, 0xFF, 0x03, 0x00};
  this->ld2450_.feed(stream);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);

  // The reverse mix is rejected the same way
  this->ld2450_.feed({0xAA, 0xFC, 0xFB, 0xFA});
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);

  // A real frame right after is still parsed
  this->ld2450_.feed(make_periodic_frame());
  EXPECT_EQ(frames, 1);
  EXPECT_EQ(this->ld2450_.reader_.length(), 0u);
}

}  // namespace esphome::ld2450::testing
