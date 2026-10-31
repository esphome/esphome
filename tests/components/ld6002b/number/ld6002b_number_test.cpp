#ifdef USE_HOST
#include "../common.h"

namespace esphome::ld6002b::testing {

TEST_F(LD6002BTest, DelayReportPublishesHoldDelay) {
  this->feed_(tf_frame(TYPE_REPORT_DELAY, u32_payload(300)));
  EXPECT_FLOAT_EQ(this->hold_delay_.state, 300.0f);
}

TEST_F(LD6002BTest, ZRangeReportPublishesLittleEndianFloats) {
  std::vector<uint8_t> p;
  put_f32(p, -0.5f);
  put_f32(p, 2.75f);
  this->feed_(tf_frame(TYPE_REPORT_Z_RANGE, p));
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

// A full command queue drops the write, and the number keeps showing the module's setting
TEST_F(LD6002BTest, HoldDelayKeepsValueWhenQueueFull) {
  this->feed_(tf_frame(TYPE_REPORT_DELAY, u32_payload(30)));
  for (int i = 0; i < 32; i++)
    this->trigger_speed_.make_call().set_index(0).perform();
  this->hold_delay_.make_call().set_value(60).perform();
  EXPECT_FLOAT_EQ(this->hold_delay_.state, 30.0f);
}

}  // namespace esphome::ld6002b::testing

#endif  // USE_HOST
