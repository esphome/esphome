#ifdef USE_HOST
#include "../common.h"

namespace esphome::ld6004::testing {

TEST_F(LD6004Test, TargetReportSetsPresence) {
  this->feed_(target_report(1));
  EXPECT_TRUE(this->presence_.state);
  EXPECT_TRUE(this->target_presence_[0].state);
  EXPECT_FALSE(this->target_presence_[1].state);

  this->feed_(target_report(0));
  EXPECT_FALSE(this->presence_.state);
  EXPECT_FALSE(this->target_presence_[0].state);
}

TEST_F(LD6004Test, AreaPresenceReportSetsAreaAndPresence) {
  this->feed_(area_presence({0, 0, 1, 0}));
  EXPECT_FALSE(this->area_presence_[0].state);
  EXPECT_TRUE(this->area_presence_[2].state);
  EXPECT_TRUE(this->presence_.state);
}

TEST_F(LD6004Test, IdleWorkStatusClearsAreaPresence) {
  this->feed_(area_presence({0, 1, 0, 0}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_STATUS, {0x00}));
  EXPECT_FALSE(this->area_presence_[1].state);
  EXPECT_FALSE(this->presence_.state);
}

TEST_F(LD6004Test, IdleWorkStatusKeepsAreaPresenceWhileTracking) {
  this->feed_(target_report(1));
  this->feed_(area_presence({0, 1, 0, 0}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_STATUS, {0x00}));
  EXPECT_TRUE(this->area_presence_[1].state);
  EXPECT_TRUE(this->presence_.state);
}

TEST_F(LD6004Test, BusyWorkStatusKeepsAreaPresence) {
  this->feed_(area_presence({0, 1, 0, 0}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_STATUS, {0x01}));
  EXPECT_TRUE(this->area_presence_[1].state);
}

class LD6004WorkModeTest : public LD6004Test, public ::testing::WithParamInterface<uint8_t> {};

// Only the two radar-off modes stop detection; the module sends the mode as one byte or as a uint32
TEST_P(LD6004WorkModeTest, RadarOffModesClearPresence) {
  const uint8_t mode = GetParam();
  const bool radar_off = mode == WORK_MODE_RADAR_OFF_P20_HIGH || mode == WORK_MODE_RADAR_OFF_P20_LOW;
  for (bool wide : {false, true}) {
    this->feed_(work_mode(WORK_MODE_NORMAL));
    this->feed_(target_report(2));
    this->feed_(area_presence({1, 0, 0, 1}));
    this->feed_(wide ? tf_frame(TYPE_REPORT_WORK_MODE, u32_payload(mode)) : work_mode(mode));
    EXPECT_EQ(this->presence_.state, !radar_off);
    EXPECT_EQ(this->target_presence_[0].state, !radar_off);
    EXPECT_EQ(this->target_presence_[1].state, !radar_off);
    EXPECT_EQ(this->area_presence_[0].state, !radar_off);
    EXPECT_EQ(this->area_presence_[3].state, !radar_off);
    // Cleared to off, not to unknown
    EXPECT_TRUE(this->presence_.has_state());
    EXPECT_TRUE(this->target_presence_[0].has_state());
    EXPECT_TRUE(this->area_presence_[0].has_state());
  }
}

INSTANTIATE_TEST_SUITE_P(Modes, LD6004WorkModeTest,
                         ::testing::Values(WORK_MODE_NORMAL, WORK_MODE_LOW_POWER, WORK_MODE_RADAR_OFF_P20_HIGH,
                                           WORK_MODE_RADAR_OFF_P20_LOW, WORK_MODE_HIGH_REFLECTIVITY));

TEST_F(LD6004Test, TrailingReportsDoNotRestorePresenceWhileRadarIsOff) {
  this->feed_(target_report(1));
  this->feed_(area_presence({0, 1, 0, 0}));
  this->feed_(work_mode(WORK_MODE_RADAR_OFF_P20_LOW));
  this->feed_(target_report(1));
  this->feed_(area_presence({0, 1, 0, 0}));
  EXPECT_FALSE(this->presence_.state);
  EXPECT_FALSE(this->target_presence_[0].state);
  EXPECT_FALSE(this->area_presence_[1].state);
}

TEST_F(LD6004Test, ReportsCountAgainOnceRadarIsBackOn) {
  this->feed_(work_mode(WORK_MODE_RADAR_OFF_P20_HIGH));
  this->feed_(work_mode(WORK_MODE_HIGH_REFLECTIVITY));
  this->feed_(target_report(1));
  EXPECT_TRUE(this->target_presence_[0].state);
  this->feed_(target_report(0));
  this->feed_(area_presence({0, 0, 1, 0}));
  EXPECT_TRUE(this->area_presence_[2].state);
  EXPECT_TRUE(this->presence_.state);
}

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
