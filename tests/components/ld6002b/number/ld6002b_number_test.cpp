#include "../common.h"

namespace esphome::ld6002b::testing {

TEST_F(LD6002BTest, DelayReportPublishesHoldDelay) {
  this->feed(tf_frame(TYPE_REPORT_DELAY, u32_payload(300)));
  EXPECT_FLOAT_EQ(this->hold_delay_.state, 300.0f);
}

TEST_F(LD6002BTest, ZRangeReportPublishesLittleEndianFloats) {
  std::vector<uint8_t> p;
  put_f32(p, -0.5f);
  put_f32(p, 2.75f);
  this->feed(tf_frame(TYPE_REPORT_Z_RANGE, p));
  EXPECT_FLOAT_EQ(this->z_min_.state, -0.5f);
  EXPECT_FLOAT_EQ(this->z_max_.state, 2.75f);
}

TEST_F(LD6002BTest, ShortZRangeReportIsIgnored) {
  this->feed(tf_frame(TYPE_REPORT_Z_RANGE, {0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00}));
  EXPECT_FALSE(this->z_min_.has_state());
}

TEST_F(LD6002BTest, LowPowerSleepReportPublishes) {
  this->feed(tf_frame(TYPE_REPORT_LOW_POWER_SLEEP, u32_payload(5000)));
  EXPECT_FLOAT_EQ(this->low_power_sleep_.state, 5000.0f);
}

TEST_F(LD6002BTest, ReportOutsideTheNumberRangeIsClamped) {
  this->feed(tf_frame(TYPE_REPORT_LOW_POWER_SLEEP, u32_payload(70000)));
  EXPECT_FLOAT_EQ(this->low_power_sleep_.state, 60000.0f);
}

}  // namespace esphome::ld6002b::testing
