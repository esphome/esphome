#ifdef USE_HOST
#include "common.h"

namespace esphome::ld6004::testing {

TEST_F(LD6004Test, BootStartsTargetStreamFirst) {
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID, TYPE_CONTROL, u32_payload(CMD_TARGET_DISPLAY_ON)));
}

// With only presence configured, boot stops the point cloud and sends no model query
TEST_F(LD6004Test, BootStopsPointCloudThenGoesQuiet) {
  this->uart_.tx.clear();
  this->feed_(ack(TYPE_CONTROL));
  EXPECT_EQ(this->uart_.tx, command(FIRST_COMMAND_ID + 1, TYPE_CONTROL, u32_payload(CMD_POINT_CLOUD_OFF)));
  this->uart_.tx.clear();
  this->feed_(ack(TYPE_CONTROL));
  EXPECT_TRUE(this->uart_.tx.empty());
}

TEST_F(LD6004Test, ReportsSendNothing) {
  this->uart_.tx.clear();
  this->feed_(work_mode(WORK_MODE_RADAR_OFF_P20_HIGH));
  this->feed_(tf_frame(TYPE_REPORT_WORK_STATUS, {0x00}));
  this->feed_(target_report(1));
  EXPECT_TRUE(this->uart_.tx.empty());
}

// A malformed model report is claimed and dropped, and the next frame still parses
TEST_F(LD6004Test, MalformedWorkModeReportIsIgnored) {
  this->feed_(target_report(1));
  this->feed_(work_mode(WORK_MODE_COUNT));
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {WORK_MODE_RADAR_OFF_P20_HIGH, 0x00}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {}));
  EXPECT_TRUE(this->presence_.state);
  this->feed_(target_report(0));
  EXPECT_FALSE(this->presence_.state);
}

TEST_F(LD6004Test, EmptyWorkStatusReportIsIgnored) {
  this->feed_(area_presence({1, 0, 0, 0}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_STATUS, {}));
  EXPECT_TRUE(this->area_presence_[0].state);
}

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
