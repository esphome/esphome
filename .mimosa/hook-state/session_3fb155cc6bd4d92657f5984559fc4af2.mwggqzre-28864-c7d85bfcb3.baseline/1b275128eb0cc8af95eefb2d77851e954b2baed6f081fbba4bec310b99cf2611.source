#include "common.h"

namespace esphome::dfrobot_sen0609::testing {

class SEN0609FrameTest : public SEN0609Test {
 protected:
  SEN0609FrameTest() : SEN0609Test(RunMode::RUN_MODE_SPEED_AND_DISTANCE) {}

  void SetUp() override {
    SEN0609Test::SetUp();
    this->hub_.set_target_distance_sensor(&this->distance_);
    this->hub_.set_target_speed_sensor(&this->speed_);
    this->hub_.set_target_energy_sensor(&this->energy_);
  }

  sensor::Sensor distance_;
  sensor::Sensor speed_;
  sensor::Sensor energy_;
};

TEST_F(SEN0609FrameTest, PresenceFrames) {
  this->feed_("$DFHPD,1, , , *\r\n");
  EXPECT_TRUE(this->has_target_.state);
  this->feed_("$DFHPD,0, , , *\r\n");
  EXPECT_FALSE(this->has_target_.state);
}

TEST_F(SEN0609FrameTest, TargetFrame) {
  this->feed_("$DFDMD,1,0,2.35,-0.40,1234,0,0*\r\n");
  EXPECT_TRUE(this->has_target_.state);
  EXPECT_FLOAT_EQ(this->distance_.state, 2.35f);
  EXPECT_FLOAT_EQ(this->speed_.state, -0.40f);
  EXPECT_FLOAT_EQ(this->energy_.state, 1234.0f);
}

TEST_F(SEN0609FrameTest, NoTargetMakesValuesUnknown) {
  this->feed_("$DFDMD,1,0,2.35,-0.40,1234,0,0*\r\n");
  this->feed_("$DFDMD,0,0,0,0,0,0,0*\r\n");
  EXPECT_FALSE(this->has_target_.state);
  EXPECT_TRUE(std::isnan(this->distance_.state));
  EXPECT_TRUE(std::isnan(this->speed_.state));
  EXPECT_TRUE(std::isnan(this->energy_.state));
}

TEST_F(SEN0609FrameTest, MalformedFramesAreIgnored) {
  this->feed_("$DFHPD,x, , , *\r\n");
  this->feed_("$DFDMD,1,0,abc,0.1,5*\r\n");
  this->feed_("$DFDMD,1,0,2.0*\r\n");
  this->feed_("$DFDMD,,0,2.0,0.1,5*\r\n");
  EXPECT_FALSE(this->has_target_.has_state());
  EXPECT_FALSE(this->distance_.has_state());
}

TEST_F(SEN0609FrameTest, FrameAfterPromptOnSameLine) {
  this->feed_("DFRobot:/>$DFHPD,1, , , *\r\n");
  EXPECT_TRUE(this->has_target_.state);
}

TEST_F(SEN0609FrameTest, FrameSplitAcrossReads) {
  this->feed_("$DFDMD,1,0,3.");
  EXPECT_FALSE(this->distance_.has_state());
  this->feed_("50,0.00,77,0,0*\r\n");
  EXPECT_FLOAT_EQ(this->distance_.state, 3.5f);
}

TEST_F(SEN0609FrameTest, LongLineIsDroppedAndParserRecovers) {
  this->feed_(std::string(200, 'x') + "\r\n");
  this->feed_("$DFHPD,1, , , *\r\n");
  EXPECT_TRUE(this->has_target_.state);
}

}  // namespace esphome::dfrobot_sen0609::testing
