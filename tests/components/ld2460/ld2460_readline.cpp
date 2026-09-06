#include "common.h"

namespace esphome::ld2460::testing {

class LD2460ReadlineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2460_.set_uart_parent(&this->mock_uart_);
    ASSERT_EQ(this->ld2460_.buffer_pos_, 0);
  }

  MockUARTComponent mock_uart_;
  TestableLD2460 ld2460_;
};

TEST_F(LD2460ReadlineTest, ValidPeriodicFrame) {
  auto frame = make_periodic_frame();
  this->ld2460_.feed(frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, ValidCommandAckFrame) {
  auto frame = make_ack_frame();
  this->ld2460_.feed(frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, BackToBackPeriodicFrames) {
  auto frame = make_periodic_frame();
  for (int i = 0; i < 5; i++) {
    this->ld2460_.feed(frame);
    EXPECT_EQ(this->ld2460_.buffer_pos_, 0) << "Frame " << i << " not processed";
  }
}

TEST_F(LD2460ReadlineTest, BackToBackMixedFrames) {
  auto periodic = make_periodic_frame();
  auto ack = make_ack_frame();
  this->ld2460_.feed(periodic);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
  this->ld2460_.feed(ack);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
  this->ld2460_.feed(periodic);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, GarbageThenValidFrame) {
  std::vector<uint8_t> garbage = {0x01, 0x02, 0x03, 0x42, 0x99};
  this->ld2460_.feed(garbage);
  EXPECT_GT(this->ld2460_.buffer_pos_, 0);

  auto frame = make_periodic_frame();
  this->ld2460_.feed(frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, FooterInGarbageResyncs) {
  std::vector<uint8_t> garbage_with_footer = {0x01, 0x02, 0x03, 0x04, 0xF8, 0xF7, 0xF6, 0xF5};
  this->ld2460_.feed(garbage_with_footer);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);

  auto frame = make_periodic_frame();
  this->ld2460_.feed(frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, CmdFooterInGarbageResyncs) {
  std::vector<uint8_t> garbage_with_footer = {0x10, 0x20, 0x30, 0x40, 0x04, 0x03, 0x02, 0x01};
  this->ld2460_.feed(garbage_with_footer);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);

  auto frame = make_periodic_frame();
  this->ld2460_.feed(frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, OverflowResetsBuffer) {
  std::vector<uint8_t> overflow_data(MAX_LINE_LENGTH, 0x11);
  this->ld2460_.feed(overflow_data);
  EXPECT_LT(this->ld2460_.buffer_pos_, 4);
}

TEST_F(LD2460ReadlineTest, OverflowThenValidFrame) {
  std::vector<uint8_t> overflow_data(MAX_LINE_LENGTH, 0x11);
  this->ld2460_.feed(overflow_data);

  auto frame = make_periodic_frame();
  this->ld2460_.feed(frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, BufferLargeEnoughForDesyncedFooter) {
  std::vector<uint8_t> mid_frame = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
  auto frame = make_periodic_frame(5);  // 31 bytes
  mid_frame.insert(mid_frame.end(), frame.begin(), frame.end());

  this->ld2460_.feed(mid_frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, SimulatedRestartThenFrames) {
  std::vector<uint8_t> restart_noise = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};
  auto frame = make_periodic_frame();
  restart_noise.insert(restart_noise.end(), frame.begin(), frame.end());

  this->ld2460_.feed(restart_noise);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);

  this->ld2460_.feed(frame);
  EXPECT_EQ(this->ld2460_.buffer_pos_, 0);
}

TEST_F(LD2460ReadlineTest, TargetCoordinateDecoding) {
  // 1 target at (1.5, 2.3)
  auto frame = make_periodic_frame(1);
  this->ld2460_.feed(frame);

  EXPECT_NEAR(this->ld2460_.target_info_[0].x, 1.5f, 0.01f);
  EXPECT_NEAR(this->ld2460_.target_info_[0].y, 2.3f, 0.01f);
  EXPECT_NEAR(this->ld2460_.target_info_[0].distance, 2.745f, 0.01f);
  // Unused slots are cleared
  EXPECT_EQ(this->ld2460_.target_info_[1].x, 0.0f);
}

TEST_F(LD2460ReadlineTest, ZeroTargetsFrameClearsInfo) {
  // First feed 1 target
  auto frame1 = make_periodic_frame(1);
  this->ld2460_.feed(frame1);
  EXPECT_NEAR(this->ld2460_.target_info_[0].x, 1.5f, 0.01f);

  // Then feed 0 targets
  auto frame0 = make_periodic_frame(0);
  this->ld2460_.feed(frame0);
  EXPECT_EQ(this->ld2460_.target_info_[0].x, 0.0f);
  EXPECT_EQ(this->ld2460_.target_info_[0].y, 0.0f);
  EXPECT_EQ(this->ld2460_.target_info_[0].distance, 0.0f);
}

TEST_F(LD2460ReadlineTest, AckQueryDetectionRange) {
  // Receipt for CMD_QUERY_DETECTION_RANGE (0x12): distance=6.0m (60), start=-50.0 deg (-500), end=50.0 deg (500)
  // FD FC FB FA 12 10 00 3C 0C FE F4 01 04 03 02 01
  std::vector<uint8_t> ack = {0xFD, 0xFC, 0xFB, 0xFA, 0x12, 0x10, 0x00, 0x3C,
                              0x0C, 0xFE, 0xF4, 0x01, 0x04, 0x03, 0x02, 0x01};
  this->ld2460_.feed(ack);
  EXPECT_NEAR(this->ld2460_.detection_distance_, 6.0f, 0.01f);
}

}  // namespace esphome::ld2460::testing
