#ifdef USE_HOST
#include "../common.h"

namespace esphome::ld6002b::testing {

TEST_F(LD6002BTest, SensitivityReportPublishesOption) {
  this->feed_(tf_frame(TYPE_REPORT_SENSITIVITY, {0x02}));
  EXPECT_EQ(this->sensitivity_.current_option(), "high");
}

TEST_F(LD6002BTest, OutOfRangeSensitivityIsIgnored) {
  this->feed_(tf_frame(TYPE_REPORT_SENSITIVITY, {0x01}));
  this->feed_(tf_frame(TYPE_REPORT_SENSITIVITY, {0x03}));
  EXPECT_EQ(this->sensitivity_.current_option(), "medium");
}

TEST_F(LD6002BTest, TriggerSpeedReportPublishesOption) {
  this->feed_(tf_frame(TYPE_REPORT_TRIGGER, {0x00}));
  EXPECT_EQ(this->trigger_speed_.current_option(), "slow");
}

TEST_F(LD6002BTest, InstallationReportPublishesOption) {
  this->feed_(tf_frame(TYPE_REPORT_INSTALLATION, {0x01}));
  EXPECT_EQ(this->installation_.current_option(), "side");
  this->feed_(tf_frame(TYPE_REPORT_INSTALLATION, {0x02}));
  EXPECT_EQ(this->installation_.current_option(), "side");
}

// A full command queue drops the write, and the select keeps showing the module's setting
TEST_F(LD6002BTest, SensitivityKeepsOptionWhenQueueFull) {
  this->feed_(tf_frame(TYPE_REPORT_SENSITIVITY, {0x01}));
  for (int i = 0; i < 32; i++)
    this->trigger_speed_.make_call().set_index(0).perform();
  this->sensitivity_.make_call().set_index(2).perform();
  EXPECT_EQ(this->sensitivity_.current_option(), "medium");
}

}  // namespace esphome::ld6002b::testing

#endif  // USE_HOST
