#include "../common.h"

namespace esphome::ld6002b::testing {

namespace {

std::vector<uint8_t> area_presence(const std::array<uint32_t, AREA_COUNT> &states) {
  std::vector<uint8_t> p;
  for (uint32_t s : states)
    put_u32(p, s);
  return tf_frame(TYPE_REPORT_AREA_PRESENCE, p);
}

}  // namespace

TEST_F(LD6002BTest, TargetReportSetsPresence) {
  this->feed_(target_report({{1.0f, 1.0f, 1.0f, 0, 4}}));
  EXPECT_TRUE(this->presence_.state);
  EXPECT_TRUE(this->target_presence_[0].state);
  EXPECT_FALSE(this->target_presence_[1].state);
  EXPECT_TRUE(this->target_presence_[1].has_state());

  this->feed_(target_report({}));
  EXPECT_FALSE(this->presence_.state);
  EXPECT_FALSE(this->target_presence_[0].state);
}

TEST_F(LD6002BTest, AreaPresenceReportSetsAreaAndPresence) {
  this->feed_(area_presence({0, 0, 1, 0}));
  EXPECT_FALSE(this->area_presence_[0].state);
  EXPECT_TRUE(this->area_presence_[2].state);
  EXPECT_TRUE(this->presence_.state);

  this->feed_(area_presence({0, 0, 0, 0}));
  EXPECT_FALSE(this->area_presence_[2].state);
  EXPECT_FALSE(this->presence_.state);
}

TEST_F(LD6002BTest, AreaPresenceKeepsPresenceWhileTargetsAreGone) {
  this->feed_(area_presence({1, 0, 0, 0}));
  this->feed_(target_report({}));
  EXPECT_TRUE(this->presence_.state);
}

TEST_F(LD6002BTest, ShortAreaPresenceReportIsIgnored) {
  this->feed_(tf_frame(TYPE_REPORT_AREA_PRESENCE, std::vector<uint8_t>(15, 1)));
  EXPECT_FALSE(this->area_presence_[0].has_state());
}

TEST_F(LD6002BTest, UnattendedWorkModeClearsAreaPresence) {
  this->feed_(area_presence({0, 1, 0, 0}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {0x00}));
  EXPECT_FALSE(this->area_presence_[1].state);
  EXPECT_FALSE(this->presence_.state);
}

TEST_F(LD6002BTest, UnattendedWorkModeKeepsAreaPresenceWhileTracking) {
  this->feed_(target_report({{1.0f, 1.0f, 1.0f, 0, 4}}));
  this->feed_(area_presence({0, 1, 0, 0}));
  this->feed_(tf_frame(TYPE_REPORT_WORK_MODE, {0x00}));
  EXPECT_TRUE(this->area_presence_[1].state);
}

}  // namespace esphome::ld6002b::testing
