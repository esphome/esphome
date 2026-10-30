#include "common.h"

namespace esphome::ld6002b::testing {

namespace {

// One target at x, so every accepted report shows up on target_x_[0]
std::vector<uint8_t> target_at(float x) { return target_report({{x, 0.5f, 1.0f, 0, 3}}); }

}  // namespace

TEST_F(LD6002BTest, BootStartsTargetStreamFirst) {
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID, TYPE_CONTROL, u32_payload(CMD_TARGET_DISPLAY_ON)));
}

TEST_F(LD6002BTest, AckReleasesNextQueuedCommand) {
  this->uart_.tx.clear();
  this->feed(ack(TYPE_CONTROL));
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID + 1, TYPE_CONTROL, u32_payload(CMD_POINT_CLOUD_ON)));
}

TEST_F(LD6002BTest, ReportDoesNotReleaseNextCommand) {
  this->uart_.tx.clear();
  this->feed(target_at(1.0f));
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(LD6002BTest, AckOfAnotherTypeDoesNotReleaseNextCommand) {
  this->uart_.tx.clear();
  this->feed(ack(TYPE_SET_HOLD_DELAY));
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(LD6002BTest, UnansweredCommandIsResentWithNewId) {
  this->uart_.tx.clear();
  // The first attempt waits 600 ms, since a sleeping module only wakes on it
  std::this_thread::sleep_for(std::chrono::milliseconds(650));
  this->feed({});
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID + 1, TYPE_CONTROL, u32_payload(CMD_TARGET_DISPLAY_ON)));
}

TEST_F(LD6002BTest, SelectWritesControlCommand) {
  const uint16_t id = this->drain_commands();
  this->sensitivity_.make_call().set_index(2).perform();
  EXPECT_EQ(this->uart_.tx, command(id, TYPE_CONTROL, u32_payload(CMD_SENSITIVITY_HIGH)));
}

TEST_F(LD6002BTest, HoldDelayNumberWritesLittleEndianValue) {
  const uint16_t id = this->drain_commands();
  this->hold_delay_.make_call().set_value(300).perform();
  EXPECT_EQ(this->uart_.tx, command(id, TYPE_SET_HOLD_DELAY, {0x2C, 0x01, 0x00, 0x00}));
}

TEST_F(LD6002BTest, ZRangeWritesBothBoundsAsLittleEndianFloats) {
  this->feed(tf_frame(TYPE_REPORT_Z_RANGE, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}));
  const uint16_t id = this->drain_commands();
  this->z_max_.make_call().set_value(2.5f).perform();
  // 0.0f and 2.5f (0x40200000)
  EXPECT_EQ(this->uart_.tx, command(id, TYPE_SET_Z_RANGE, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40}));
}

TEST_F(LD6002BTest, BadHeaderChecksumIsDropped) {
  auto bad = target_at(1.0f);
  bad[7] ^= 0x10;
  this->feed(bad);
  EXPECT_TRUE(this->target_x0_published_.empty());
  this->feed(target_at(2.0f));
  ASSERT_EQ(this->target_x0_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->target_x0_published_[0], 2.0f);
}

TEST_F(LD6002BTest, BadDataChecksumIsDroppedAndNextFrameRecovers) {
  auto bad = target_at(1.0f);
  bad.back() ^= 0x10;
  this->feed(bad);
  EXPECT_TRUE(this->target_x0_published_.empty());
  this->feed(target_at(2.0f));
  ASSERT_EQ(this->target_x0_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->target_x0_published_[0], 2.0f);
}

TEST_F(LD6002BTest, CorruptPayloadByteIsDropped) {
  auto bad = target_at(1.0f);
  bad[ld600x::TF_HEADER_LEN + 2 + 4] ^= 0x01;
  this->feed(bad);
  EXPECT_TRUE(this->target_x0_published_.empty());
}

TEST_F(LD6002BTest, GarbageBetweenFramesIsSkipped) {
  this->feed(concat(concat(target_at(1.0f), {0x00, 0xFF, 0x55, 0xAA}), target_at(2.0f)));
  ASSERT_EQ(this->target_x0_published_.size(), 2u);
  EXPECT_FLOAT_EQ(this->target_x0_published_[0], 1.0f);
  EXPECT_FLOAT_EQ(this->target_x0_published_[1], 2.0f);
}

TEST_F(LD6002BTest, BackToBackFramesInOneRead) {
  this->feed(concat(target_at(1.0f), target_at(2.0f)));
  ASSERT_EQ(this->target_x0_published_.size(), 2u);
  EXPECT_FLOAT_EQ(this->target_x0_published_[1], 2.0f);
}

TEST_F(LD6002BTest, FrameSplitAcrossLoopsIsJoined) {
  const auto f = target_at(4.0f);
  // Split inside the header and again inside the payload
  this->feed(std::vector<uint8_t>(f.begin(), f.begin() + 3));
  this->feed(std::vector<uint8_t>(f.begin() + 3, f.begin() + 12));
  EXPECT_TRUE(this->target_x0_published_.empty());
  this->feed(std::vector<uint8_t>(f.begin() + 12, f.end()));
  ASSERT_EQ(this->target_x0_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->target_x0_published_[0], 4.0f);
}

// The point_count sensor sizes the buffer at 4096; a longer payload is skipped by length, SOF bytes and all
TEST_F(LD6002BTest, OversizedFrameIsSkippedWithoutLosingNextFrame) {
  this->feed(concat(tf_frame(TYPE_REPORT_TARGET, std::vector<uint8_t>(5000, ld600x::TF_SOF)), target_at(3.0f)));
  ASSERT_EQ(this->target_x0_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->target_x0_published_[0], 3.0f);
}

TEST_F(LD6002BTest, UnknownTypeIsIgnored) {
  this->uart_.tx.clear();
  this->feed(tf_frame(0x0A99, {0x01, 0x02, 0x03, 0x04}));
  this->feed(tf_frame(0x0A99, {}));
  EXPECT_TRUE(this->uart_.tx.empty());
  this->feed(target_at(5.0f));
  ASSERT_EQ(this->target_x0_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->target_x0_published_[0], 5.0f);
}

TEST_F(LD6002BTest, ShortReportIsIgnored) {
  this->feed(tf_frame(TYPE_REPORT_TARGET, {0x01, 0x00}));
  EXPECT_TRUE(this->target_count_published_.empty());
  EXPECT_FALSE(this->presence_.has_state());
}

}  // namespace esphome::ld6002b::testing
