#include "common.h"

namespace esphome::dfrobot_sen0609::testing {

class SEN0609SequencerTest : public SEN0609Test {
 protected:
  void SetUp() override {
    SEN0609Test::SetUp();
    this->hub_.set_version_text_sensor(&this->version_);
    this->hub_.set_hardware_version_text_sensor(&this->hardware_version_);
  }

  // Runs the boot sequence with a module that answers everything
  void boot_() {
    this->hub_.setup();
    this->expect_and_answer_("sensorStop");
    this->expect_and_answer_("setRunApp 0");
    this->expect_and_answer_("setUartOutput 1 1 1 1.0");
    this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
    this->expect_and_answer_("getHWV", "HardwareVersion:V2.0\r\n");
    this->expect_and_answer_("sensorStart");
  }

  text_sensor::TextSensor version_;
  text_sensor::TextSensor hardware_version_;
};

TEST_F(SEN0609SequencerTest, BootAppliesModeAndReadsVersions) {
  this->boot_();
  EXPECT_EQ(this->version_.state, "V1.0.4");
  EXPECT_EQ(this->hardware_version_.state, "V2.0");
  EXPECT_EQ(this->next_command_(), "");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, ErrorAnswerIsRetriedThenSkipped) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "getSWV");
    this->feed_("getSWV\r\nError\r\nDFRobot:/>");
  }
  this->expect_and_answer_("getHWV", "HardwareVersion:V2.0\r\n");
  this->expect_and_answer_("sensorStart");
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_FALSE(this->version_.has_state());
}

TEST_F(SEN0609SequencerTest, SilentModuleBacksOffAndKeepsWork) {
  this->hub_.setup();
  for (int i = 0; i < 3; i++)
    ASSERT_EQ(this->next_command_(), "sensorStop");
  this->run_(2100);  // the last attempt times out
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_EQ(this->next_command_(20000), "");
  // The module shows up later; the boot work is still pending
  ASSERT_EQ(this->next_command_(15000), "sensorStop");
  this->answer_("sensorStop");
  EXPECT_EQ(this->next_command_(), "setRunApp 0");
}

TEST_F(SEN0609SequencerTest, AlreadyStoppedCountsAsStopped) {
  this->hub_.setup();
  ASSERT_EQ(this->next_command_(), "sensorStop");
  this->feed_("sensorStop\r\nsensor stopped already\r\nDFRobot:/>");
  EXPECT_EQ(this->next_command_(), "setRunApp 0");
}

}  // namespace esphome::dfrobot_sen0609::testing
