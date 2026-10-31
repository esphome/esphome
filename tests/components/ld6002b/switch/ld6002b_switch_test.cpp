#ifdef USE_HOST
#include "../common.h"

namespace esphome::ld6002b::testing {

TEST_F(LD6002BTest, LowPowerReportPublishesSwitchState) {
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER, {0x01}));
  EXPECT_TRUE(this->low_power_.state);
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER, {0x00}));
  EXPECT_FALSE(this->low_power_.state);
}

TEST_F(LD6002BTest, EmptyLowPowerReportIsIgnored) {
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER, {0x01}));
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER, {}));
  EXPECT_TRUE(this->low_power_.state);
}

TEST_F(LD6002BTest, LowPowerSwitchKeepsStateWhenQueueFull) {
  for (int i = 0; i < 32; i++)
    this->sensitivity_.make_call().set_index(0).perform();
  this->low_power_.turn_on();
  EXPECT_FALSE(this->low_power_.state);
}

}  // namespace esphome::ld6002b::testing

#endif  // USE_HOST
