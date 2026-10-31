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

}  // namespace esphome::ld6002b::testing

#endif  // USE_HOST
