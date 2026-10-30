#include "../common.h"

namespace esphome::ld6002b::testing {

TEST_F(LD6002BTest, VersionReportPublishesVersion) {
  this->feed_(tf_frame(TYPE_QUERY_VERSION, {0x00, 0x01, 0x02, 0x03}));
  EXPECT_EQ(this->ota_version_.get_state(), "1.2.3");
}

TEST_F(LD6002BTest, VersionReportNamesTheProject) {
  this->feed_(tf_frame(TYPE_QUERY_VERSION, {0x02, 0x01, 0x00, 0x0A}));
  EXPECT_EQ(this->ota_version_.get_state(), "p2 1.0.10");
}

TEST_F(LD6002BTest, ShortVersionReportIsIgnored) {
  this->feed_(tf_frame(TYPE_QUERY_VERSION, {0x00, 0x01, 0x02}));
  EXPECT_FALSE(this->ota_version_.has_state());
}

TEST_F(LD6002BTest, WorkModeReportPublishesMode) {
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {0x00}));
  EXPECT_EQ(this->work_mode_.get_state(), "low_power");
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {0x01}));
  EXPECT_EQ(this->work_mode_.get_state(), "normal");
}

TEST_F(LD6002BTest, WorkModeFallsBackToLowPowerReport) {
  this->feed_(tf_frame(TYPE_REPORT_LOW_POWER, {0x01}));
  EXPECT_EQ(this->work_mode_.get_state(), "low_power");
  this->feed_(target_report({{1.0f, 1.0f, 1.0f, 0, 4}}));
  EXPECT_EQ(this->work_mode_.get_state(), "normal");
}

}  // namespace esphome::ld6002b::testing
