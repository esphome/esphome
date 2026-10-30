#include "../common.h"

namespace esphome::ld6002b::testing {

TEST_F(LD6002BTest, LowPowerReportPublishesSwitchState) {
  this->feed(tf_frame(TYPE_REPORT_LOW_POWER, {0x01}));
  EXPECT_TRUE(this->low_power_.state);
  this->feed(tf_frame(TYPE_REPORT_LOW_POWER, {0x00}));
  EXPECT_FALSE(this->low_power_.state);
}

TEST_F(LD6002BTest, EmptyLowPowerReportIsIgnored) {
  this->feed(tf_frame(TYPE_REPORT_LOW_POWER, {0x01}));
  this->feed(tf_frame(TYPE_REPORT_LOW_POWER, {}));
  EXPECT_TRUE(this->low_power_.state);
}

}  // namespace esphome::ld6002b::testing
