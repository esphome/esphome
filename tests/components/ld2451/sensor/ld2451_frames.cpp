#include <cmath>

#include "../common.h"

namespace esphome::ld2451::testing {

class LD2451FrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2451_.set_uart_parent(&this->mock_uart_);
    this->ld2451_.set_target_count_sensor(&this->count_);
    for (uint8_t i = 0; i < MAX_TARGETS; i++) {
      this->ld2451_.set_target_angle_sensor(i, &this->angle_[i]);
      this->ld2451_.set_target_distance_sensor(i, &this->distance_[i]);
      this->ld2451_.set_target_speed_sensor(i, &this->speed_[i]);
      this->ld2451_.set_target_snr_sensor(i, &this->snr_[i]);
    }
  }

  void expect_target_unknown_(uint8_t i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(std::isnan(this->angle_[i].state));
    EXPECT_TRUE(std::isnan(this->distance_[i].state));
    EXPECT_TRUE(std::isnan(this->speed_[i].state));
    EXPECT_TRUE(std::isnan(this->snr_[i].state));
  }

  // Delivers data through the mock UART and one pass of loop()
  void feed_(const std::vector<uint8_t> &data) {
    this->mock_uart_.rx.insert(this->mock_uart_.rx.end(), data.begin(), data.end());
    this->ld2451_.loop();
  }

  MockUARTComponent mock_uart_;
  LD2451Component ld2451_;
  sensor::Sensor count_;
  sensor::Sensor angle_[MAX_TARGETS];
  sensor::Sensor distance_[MAX_TARGETS];
  sensor::Sensor speed_[MAX_TARGETS];
  sensor::Sensor snr_[MAX_TARGETS];
};

// The example frame from the HLK-LD2451 serial protocol V1.03, section 1.3.1. The datasheet calls direction 0x00
// approaching, but a radar watching traffic sends 0x01 for approaching targets, so the first and third targets here
// move away.
TEST_F(LD2451FrameTest, DatasheetExample) {
  this->feed_({0xF4, 0xF3, 0xF2, 0xF1, 0x11, 0x00, 0x03, 0x01, 0x8A, 0x28, 0x00, 0x3C, 0x15, 0x8A,
               0x1E, 0x01, 0x3C, 0x0F, 0x76, 0x5F, 0x00, 0x3C, 0x0F, 0xF8, 0xF7, 0xF6, 0xF5});
  EXPECT_FLOAT_EQ(this->count_.state, 3);
  EXPECT_FLOAT_EQ(this->angle_[0].state, 10);
  EXPECT_FLOAT_EQ(this->distance_[0].state, 40);
  EXPECT_FLOAT_EQ(this->speed_[0].state, 60);
  EXPECT_FLOAT_EQ(this->snr_[0].state, 21);
  EXPECT_FLOAT_EQ(this->distance_[1].state, 30);
  EXPECT_FLOAT_EQ(this->speed_[1].state, -60);
  EXPECT_FLOAT_EQ(this->angle_[2].state, -10);
  EXPECT_FLOAT_EQ(this->distance_[2].state, 95);
  this->expect_target_unknown_(3);
  this->expect_target_unknown_(4);
}

// Captured from an LD2451 as a car drove towards it, 28 m out
TEST_F(LD2451FrameTest, ApproachingCarFromRadar) {
  this->feed_({0xF4, 0xF3, 0xF2, 0xF1, 0x07, 0x00, 0x01, 0x01, 0x7F, 0x1C, 0x01, 0x1E, 0xFF, 0xF8, 0xF7, 0xF6, 0xF5});
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_FLOAT_EQ(this->angle_[0].state, -1);
  EXPECT_FLOAT_EQ(this->distance_[0].state, 28);
  EXPECT_FLOAT_EQ(this->speed_[0].state, -30);
  EXPECT_FLOAT_EQ(this->snr_[0].state, 255);
}

struct DecodeCase {
  const char *name;
  Target target;
  float angle;
  float speed;
};

class LD2451DecodeTest : public LD2451FrameTest, public ::testing::WithParamInterface<DecodeCase> {};

TEST_P(LD2451DecodeTest, DecodesOneTarget) {
  const DecodeCase &c = GetParam();
  this->feed_(make_data_frame({c.target}));
  EXPECT_FLOAT_EQ(this->angle_[0].state, c.angle);
  EXPECT_FLOAT_EQ(this->speed_[0].state, c.speed);
  EXPECT_FLOAT_EQ(this->distance_[0].state, c.target.distance);
  EXPECT_FLOAT_EQ(this->snr_[0].state, c.target.snr);
}

INSTANTIATE_TEST_SUITE_P(Targets, LD2451DecodeTest,
                         ::testing::Values(DecodeCase{"straight_ahead", {0, 50, true, 30, 100}, 0, -30},
                                           DecodeCase{"left_edge", {-20, 1, true, 120, 255}, -20, -120},
                                           DecodeCase{"right_edge", {20, 100, false, 120, 0}, 20, 120},
                                           DecodeCase{"moving_away", {5, 10, false, 1, 7}, 5, 1},
                                           DecodeCase{"standing_still", {0, 10, true, 0, 7}, 0, 0}),
                         [](const ::testing::TestParamInfo<DecodeCase> &info) { return info.param.name; });

TEST_F(LD2451FrameTest, FiveTargets) {
  this->feed_(make_data_frame(
      {{1, 10, true, 10, 1}, {2, 20, true, 20, 2}, {3, 30, true, 30, 3}, {4, 40, true, 40, 4}, {5, 50, false, 50, 5}}));
  EXPECT_FLOAT_EQ(this->count_.state, 5);
  EXPECT_FLOAT_EQ(this->angle_[4].state, 5);
  EXPECT_FLOAT_EQ(this->distance_[4].state, 50);
  EXPECT_FLOAT_EQ(this->speed_[4].state, 50);
}

TEST_F(LD2451FrameTest, FewerTargetsClearUnusedSlots) {
  this->feed_(make_data_frame({{1, 10, true, 10, 1}, {2, 20, true, 20, 2}, {3, 30, true, 30, 3}}));
  this->feed_(make_data_frame({{4, 40, true, 40, 4}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_FLOAT_EQ(this->distance_[0].state, 40);
  this->expect_target_unknown_(1);
  this->expect_target_unknown_(2);
}

TEST_F(LD2451FrameTest, NoTargetsClearAllSlots) {
  this->feed_(make_data_frame({{1, 10, true, 10, 1}}));
  this->feed_(make_data_frame({}));
  EXPECT_FLOAT_EQ(this->count_.state, 0);
  this->expect_target_unknown_(0);
}

TEST_F(LD2451FrameTest, EmptyPayloadMeansNoTargets) {
  this->feed_(make_data_frame({{1, 10, true, 10, 1}}));
  this->feed_(make_empty_data_frame());
  EXPECT_FLOAT_EQ(this->count_.state, 0);
  this->expect_target_unknown_(0);
}

TEST_F(LD2451FrameTest, OneBytePayloadIsRejected) {
  this->feed_(make_data_frame({{1, 10, true, 10, 1}}));
  this->feed_({0xF4, 0xF3, 0xF2, 0xF1, 0x01, 0x00, 0x00, 0xF8, 0xF7, 0xF6, 0xF5});
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_FLOAT_EQ(this->distance_[0].state, 10);
}

TEST_F(LD2451FrameTest, CountThatDisagreesWithLengthIsRejected) {
  this->feed_(make_data_frame({{1, 10, true, 10, 1}}));
  auto frame = make_data_frame({{2, 20, true, 20, 2}});
  frame[6] = 2;  // two targets claimed, one carried
  this->feed_(frame);
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_FLOAT_EQ(this->distance_[0].state, 10);
}

TEST_F(LD2451FrameTest, FrameLongerThanBufferIsSkipped) {
  std::vector<Target> six(6, Target{1, 10, true, 10, 1});
  this->feed_(make_data_frame(six));
  EXPECT_TRUE(std::isnan(this->count_.state));
  this->feed_(make_data_frame({{2, 20, true, 20, 2}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
  EXPECT_FLOAT_EQ(this->distance_[0].state, 20);
}

TEST_F(LD2451FrameTest, AckFramesAreIgnored) {
  this->feed_(make_ack_frame());
  EXPECT_TRUE(std::isnan(this->count_.state));
  this->feed_(make_data_frame({{2, 20, true, 20, 2}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
}

TEST_F(LD2451FrameTest, GarbageBeforeFrame) {
  this->feed_({0x01, 0xF4, 0xF3, 0x55, 0xF8, 0xF7, 0xF6, 0xF5});
  this->feed_(make_data_frame({{2, 20, true, 20, 2}}));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
}

TEST_F(LD2451FrameTest, FrameSplitAcrossLoops) {
  auto frame = make_data_frame({{2, 20, true, 20, 2}});
  this->feed_(std::vector<uint8_t>(frame.begin(), frame.begin() + 3));
  this->feed_(std::vector<uint8_t>(frame.begin() + 3, frame.begin() + 9));
  EXPECT_TRUE(std::isnan(this->count_.state));
  this->feed_(std::vector<uint8_t>(frame.begin() + 9, frame.end()));
  EXPECT_FLOAT_EQ(this->count_.state, 1);
}

TEST_F(LD2451FrameTest, BadFooterIsRejected) {
  auto frame = make_data_frame({{2, 20, true, 20, 2}});
  frame.back() = 0x00;
  this->feed_(frame);
  EXPECT_TRUE(std::isnan(this->count_.state));
}

}  // namespace esphome::ld2451::testing
