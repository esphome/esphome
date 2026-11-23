#ifdef USE_HOST
#include "../common.h"

namespace esphome::ld6002b::testing {

namespace {

std::vector<uint8_t> z_payload(float z_min, float z_max) {
  std::vector<uint8_t> p;
  put_f32(p, z_min);
  put_f32(p, z_max);
  return p;
}

std::vector<uint8_t> z_range(float z_min, float z_max) {
  return tf_frame(TYPE_REPORT_Z_RANGE, z_payload(z_min, z_max));
}

}  // namespace

TEST_F(LD6002BTest, DelayReportPublishesHoldDelay) {
  this->feed_(tf_frame(TYPE_REPORT_DELAY, u32_payload(300)));
  EXPECT_FLOAT_EQ(this->hold_delay_.state, 300.0f);
}

TEST_F(LD6002BTest, ZRangeReportPublishesLittleEndianFloats) {
  this->feed_(z_range(-0.5f, 2.75f));
  EXPECT_FLOAT_EQ(this->z_min_.state, -0.5f);
  EXPECT_FLOAT_EQ(this->z_max_.state, 2.75f);
}

TEST_F(LD6002BTest, ShortZRangeReportIsIgnored) {
  this->feed_(tf_frame(TYPE_REPORT_Z_RANGE, {0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00}));
  EXPECT_FALSE(this->z_min_.has_state());
}

TEST_F(LD6002BTest, LowPowerSleepReportPublishes) {
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER_SLEEP, u32_payload(5000)));
  EXPECT_FLOAT_EQ(this->low_power_sleep_.state, 5000.0f);
}

TEST_F(LD6002BTest, ReportOutsideTheNumberRangeIsClamped) {
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER_SLEEP, u32_payload(70000)));
  EXPECT_FLOAT_EQ(this->low_power_sleep_.state, 60000.0f);
}

// The module takes whole numbers only, so a fraction is rejected rather than truncated
TEST_F(LD6002BTest, HoldDelayFractionIsNotWrittenOrPublished) {
  this->feed_(tf_frame(TYPE_REPORT_DELAY, u32_payload(30)));
  const uint16_t id = this->drain_commands_();
  this->hold_delay_.make_call().set_value(12.5f).perform();
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_FLOAT_EQ(this->hold_delay_.state, 30.0f);
  this->hold_delay_.make_call().set_value(60).perform();
  EXPECT_EQ(this->uart_.tx, command(id, TYPE_SET_HOLD_DELAY, u32_payload(60)));
  EXPECT_FLOAT_EQ(this->hold_delay_.state, 60.0f);
}

// A full command queue drops the write, and the number keeps showing the module's setting
TEST_F(LD6002BTest, HoldDelayKeepsValueWhenQueueFull) {
  this->feed_(tf_frame(TYPE_REPORT_DELAY, u32_payload(30)));
  this->fill_command_queue_();
  this->hold_delay_.make_call().set_value(60).perform();
  EXPECT_FLOAT_EQ(this->hold_delay_.state, 30.0f);
}

// Crossed bounds cannot be written, so the number keeps the module's value and the next write uses the old bound
TEST_F(LD6002BTest, CrossedZRangeIsNotPublished) {
  this->feed_(z_range(-0.5f, 2.75f));
  const uint16_t id = this->drain_commands_();
  this->z_min_.make_call().set_value(5.0f).perform();
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_FLOAT_EQ(this->z_min_.state, -0.5f);
  this->z_max_.make_call().set_value(3.0f).perform();
  EXPECT_EQ(this->uart_.tx, command(id, TYPE_SET_Z_RANGE, z_payload(-0.5f, 3.0f)));
}

TEST_F(LD6002BTest, ZRangeIsNotPublishedWhenQueueFull) {
  this->feed_(z_range(-0.5f, 2.75f));
  this->fill_command_queue_();
  this->z_max_.make_call().set_value(3.0f).perform();
  EXPECT_FLOAT_EQ(this->z_max_.state, 2.75f);
}

// With the other bound unknown nothing can be written yet; the value is held and shown
TEST_F(LD6002BTest, ZBoundIsHeldAndPublishedWhileOtherIsUnknown) {
  this->drain_commands_();
  this->z_min_.make_call().set_value(1.0f).perform();
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_FLOAT_EQ(this->z_min_.state, 1.0f);
}

}  // namespace esphome::ld6002b::testing

#endif  // USE_HOST
