#include <cmath>

#include "../common.h"

namespace esphome::ld2460::testing {

class LD2460ReadlineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2460_.set_uart_parent(&this->mock_uart_);
    EXPECT_CALL(this->mock_uart_, write_array(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
    this->ld2460_.set_target_count_sensor(&this->count_);
    for (uint8_t i = 0; i < MAX_TARGETS; i++) {
      this->ld2460_.set_target_x_sensor(i, &this->x_[i]);
      this->ld2460_.set_target_y_sensor(i, &this->y_[i]);
      this->ld2460_.set_target_distance_sensor(i, &this->distance_[i]);
      this->ld2460_.set_target_angle_sensor(i, &this->angle_[i]);
    }
  }

  MockUARTComponent mock_uart_;
  TestableLD2460 ld2460_;
  sensor::Sensor count_;
  sensor::Sensor x_[MAX_TARGETS];
  sensor::Sensor y_[MAX_TARGETS];
  sensor::Sensor distance_[MAX_TARGETS];
  sensor::Sensor angle_[MAX_TARGETS];
};

TEST_F(LD2460ReadlineTest, BackToBackFrames) {
  this->ld2460_.feed(make_periodic_frame({{10, 10}}));
  this->ld2460_.feed(make_periodic_frame({{10, 10}, {-20, 30}}));
  EXPECT_FLOAT_EQ(this->count_.state, 2);
  EXPECT_NEAR(this->x_[1].state, -2.0f, 0.001f);
  this->ld2460_.feed(make_periodic_frame({{12, 10}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_NEAR(this->x_[0].state, 1.2f, 0.001f);
  EXPECT_TRUE(std::isnan(this->x_[1].state));
}

TEST_F(LD2460ReadlineTest, MixedFrameTypes) {
  this->ld2460_.feed(make_periodic_frame());
  this->ld2460_.feed(make_ack_frame(1));
  this->ld2460_.feed(make_periodic_frame({{-5, 40}, {5, 40}}));
  EXPECT_FLOAT_EQ(this->count_.state, 2);
  EXPECT_NEAR(this->y_[1].state, 4.0f, 0.001f);
}

TEST_F(LD2460ReadlineTest, FooterBytesInsideGarbage) {
  this->ld2460_.feed({0x01, 0x02, 0x03, 0x04, 0xF8, 0xF7, 0xF6, 0xF5});
  this->ld2460_.feed({0x10, 0x20, 0x30, 0x40, 0x04, 0x03, 0x02, 0x01});
  EXPECT_TRUE(std::isnan(this->count_.state));
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_FLOAT_EQ(this->count_.state, 1);
}

TEST_F(LD2460ReadlineTest, OverflowThenValidFrame) {
  this->ld2460_.feed(std::vector<uint8_t>(MAX_LINE_LENGTH + 8, 0x11));
  // The frame right after the overflow lands behind leftover bytes; the header resync skips them
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_NEAR(this->x_[0].state, 1.5f, 0.001f);
}

TEST_F(LD2460ReadlineTest, MaxTargetsFrame) {
  this->ld2460_.feed(make_periodic_frame({{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}}));
  EXPECT_FLOAT_EQ(this->count_.state, 5);
  EXPECT_NEAR(this->x_[4].state, 0.9f, 0.001f);
  EXPECT_NEAR(this->y_[4].state, 1.0f, 0.001f);
}

TEST_F(LD2460ReadlineTest, CoordinateDecoding) {
  this->ld2460_.feed(make_periodic_frame({{15, 23}, {-300, -1}}));
  EXPECT_NEAR(this->x_[0].state, 1.5f, 0.001f);
  EXPECT_NEAR(this->y_[0].state, 2.3f, 0.001f);
  EXPECT_NEAR(this->distance_[0].state, 2.746f, 0.001f);
  EXPECT_NEAR(this->x_[1].state, -30.0f, 0.001f);
  EXPECT_NEAR(this->y_[1].state, -0.1f, 0.001f);

  this->ld2460_.feed(make_periodic_frame({}));
  EXPECT_FLOAT_EQ(this->count_.state, 0);
  EXPECT_TRUE(std::isnan(this->x_[0].state));
  EXPECT_TRUE(std::isnan(this->distance_[0].state));
  EXPECT_TRUE(std::isnan(this->angle_[1].state));
}

TEST_F(LD2460ReadlineTest, AngleByInstallationMode) {
  // Side mount (default): atan2(x, y)
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_NEAR(this->angle_[0].state, 33.11f, 0.05f);

  // Top mount: atan2(y, x), wrapped to 0..360; the same coordinates are republished
  this->ld2460_.feed(make_ack_frame(2));
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_NEAR(this->angle_[0].state, 56.89f, 0.05f);
  this->ld2460_.feed(make_periodic_frame({{15, -23}}));
  EXPECT_NEAR(this->angle_[0].state, 303.11f, 0.05f);
}

TEST_F(LD2460ReadlineTest, AckHandling) {
  // An ACK for another command or with a bad length leaves the side mount in place
  this->ld2460_.feed({0xFD, 0xFC, 0xFB, 0xFA, 0x0B, 0x0C, 0x00, 0x02, 0x04, 0x03, 0x02, 0x01});
  this->ld2460_.feed({0xFD, 0xFC, 0xFB, 0xFA, 0x0A, 0x0D, 0x00, 0x02, 0x04, 0x03, 0x02, 0x01});
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_NEAR(this->angle_[0].state, 33.11f, 0.05f);

  this->ld2460_.feed(make_ack_frame(2));
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_NEAR(this->angle_[0].state, 56.89f, 0.05f);
}

TEST_F(LD2460ReadlineTest, QueryResentWhileNoAck) {
  // No ACK arrived: the first two data frames resend the 12-byte query (three attempts with the
  // one from setup), after which the hub gives up and assumes a side mount
  EXPECT_CALL(this->mock_uart_, write_array(::testing::_, 12)).Times(2);
  this->ld2460_.feed(make_periodic_frame());
  this->ld2460_.feed(make_periodic_frame({{20, 20}}));
  this->ld2460_.feed(make_periodic_frame({{30, 30}}));
  this->ld2460_.feed(make_periodic_frame({{40, 40}}));
}

TEST_F(LD2460ReadlineTest, InvalidModeKeepsQuerying) {
  // A mode byte that is neither side nor top is rejected and the next frame asks again
  EXPECT_CALL(this->mock_uart_, write_array(::testing::_, 12)).Times(1);
  this->ld2460_.feed(make_ack_frame(3));
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_NEAR(this->angle_[0].state, 33.11f, 0.05f);
}

TEST_F(LD2460ReadlineTest, StrayBytesBeforeFrame) {
  // Garbage in front of a frame is skipped once the footer is seen, for data and command frames
  this->ld2460_.feed({0x01, 0x02, 0x03});
  this->ld2460_.feed(make_ack_frame(2));
  this->ld2460_.feed({0xAA, 0xBB});
  this->ld2460_.feed(make_periodic_frame());
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_NEAR(this->angle_[0].state, 56.89f, 0.05f);
}

TEST_F(LD2460ReadlineTest, StrayByteThenTruncatedFrame) {
  // The truncated frame's header is not followed by a matching length, so the full frame is used
  this->ld2460_.feed({0x00, 0xF4, 0xF3, 0xF2, 0xF1, 0x04});
  this->ld2460_.feed(make_periodic_frame({{20, 30}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_NEAR(this->x_[0].state, 2.0f, 0.001f);
  EXPECT_NEAR(this->y_[0].state, 3.0f, 0.001f);
}

TEST_F(LD2460ReadlineTest, HeaderBytesInsidePayload) {
  // The target bytes F4 F3 F2 F1 repeat the data header; the frame still parses at offset 0
  this->ld2460_.feed(make_periodic_frame({{static_cast<int16_t>(0xF3F4), static_cast<int16_t>(0xF1F2)}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_NEAR(this->x_[0].state, -308.4f, 0.01f);
  EXPECT_NEAR(this->y_[0].state, -359.8f, 0.01f);
}

TEST_F(LD2460ReadlineTest, PartialTargetRejected) {
  // 13 bytes of payload is not a whole number of 4-byte targets
  std::vector<uint8_t> frame = {0xF4, 0xF3, 0xF2, 0xF1, 0x04, 0x0D, 0x00, 0x0F, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};
  this->ld2460_.feed(frame);
  EXPECT_TRUE(std::isnan(this->count_.state));
}

TEST_F(LD2460ReadlineTest, NoQueryAfterAck) {
  EXPECT_CALL(this->mock_uart_, write_array(::testing::_, ::testing::_)).Times(0);
  this->ld2460_.feed(make_ack_frame(1));
  this->ld2460_.feed(make_periodic_frame());
  this->ld2460_.feed(make_periodic_frame({{20, 20}}));
}

TEST_F(LD2460ReadlineTest, DataCallbackOncePerValidFrame) {
  int calls = 0;
  this->ld2460_.add_on_data_callback([&calls]() { calls++; });
  this->ld2460_.feed(make_periodic_frame());
  this->ld2460_.feed(make_periodic_frame({{20, 20}}));
  EXPECT_EQ(calls, 2);
  // A frame with the data footer but a wrong header is rejected without a callback
  this->ld2460_.feed({0x00, 0x00, 0x00, 0x00, 0x04, 0x0B, 0x00, 0xF8, 0xF7, 0xF6, 0xF5});
  this->ld2460_.feed(make_ack_frame(2));
  EXPECT_EQ(calls, 2);
}

TEST_F(LD2460ReadlineTest, TruncatedPrefixBeforeLargeFrames) {
  // Three targets still fit behind the truncated prefix; four and five overflow the buffer
  const std::vector<Point> targets = {{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}};
  for (int n = 3; n <= MAX_TARGETS; n++) {
    SCOPED_TRACE(n);
    this->ld2460_.feed({0x00, 0xF4, 0xF3, 0xF2, 0xF1, 0x04});
    this->ld2460_.feed(make_periodic_frame(std::vector<Point>(targets.begin(), targets.begin() + n)));
    EXPECT_FLOAT_EQ(this->count_.state, n);
    EXPECT_NEAR(this->x_[n - 1].state, (2 * n - 1) * 0.1f, 0.001f);
  }
}

TEST_F(LD2460ReadlineTest, LostFooterTwoTargetResync) {
  // A two-target frame missing its last byte; the next frame overflows the buffer
  auto broken = make_periodic_frame({{10, 10}, {20, 20}});
  broken.pop_back();
  this->ld2460_.feed(broken);
  this->ld2460_.feed(make_periodic_frame({{30, 40}, {50, 60}}));
  EXPECT_FLOAT_EQ(this->count_.state, 2);
  EXPECT_NEAR(this->x_[1].state, 5.0f, 0.001f);
  this->ld2460_.feed(make_periodic_frame({{70, 80}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_NEAR(this->x_[0].state, 7.0f, 0.001f);
}

TEST_F(LD2460ReadlineTest, HeaderLikeGarbageThenMaxTargets) {
  // Header first bytes and whole command headers in the noise do not hold back a real frame
  this->ld2460_.feed(std::vector<uint8_t>(70, 0xF4));
  this->ld2460_.feed(make_periodic_frame({{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}}));
  EXPECT_FLOAT_EQ(this->count_.state, 5);
  std::vector<uint8_t> garbage;
  for (int i = 0; i < 40; i++) {
    garbage.insert(garbage.end(), {0xFD, 0xFC, 0xFB, 0xFA});
  }
  this->ld2460_.feed(garbage);
  this->ld2460_.feed(make_periodic_frame({{11, 12}, {3, 4}, {5, 6}, {7, 8}, {9, 10}}));
  EXPECT_NEAR(this->x_[0].state, 1.1f, 0.001f);
}

}  // namespace esphome::ld2460::testing
