#include "common.h"

namespace esphome::dfrobot_sen0609::testing {

class SEN0609SequencerTest : public SEN0609Test {
 protected:
  void SetUp() override {
    SEN0609Test::SetUp();
    this->hub_.set_number(NUMBER_MIN_RANGE, &this->min_range_);
    this->hub_.set_number(NUMBER_MAX_RANGE, &this->max_range_);
    this->hub_.set_number(NUMBER_HOLD_SENSITIVITY, &this->hold_sensitivity_);
    this->hub_.set_version_text_sensor(&this->version_);
  }

  // Runs the boot sequence with a module that answers everything
  void boot_() {
    this->hub_.setup();
    this->expect_and_answer_("sensorStop");
    this->expect_and_answer_("setRunApp 0");
    this->expect_and_answer_("setUartOutput 1 1 1 1.0");
    this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
    this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
    this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
    this->expect_and_answer_("sensorStart");
  }

  TestNumber min_range_;
  TestNumber max_range_;
  TestNumber hold_sensitivity_;
  text_sensor::TextSensor version_;
};

TEST_F(SEN0609SequencerTest, BootReadsSettingsWithoutSaving) {
  this->boot_();
  EXPECT_FLOAT_EQ(this->min_range_.state, 0.6f);
  EXPECT_FLOAT_EQ(this->max_range_.state, 12.0f);
  EXPECT_FLOAT_EQ(this->hold_sensitivity_.state, 7.0f);
  EXPECT_EQ(this->version_.state, "V1.0.4");
  EXPECT_EQ(this->next_command_(), "");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, ChangesMadeTogetherShareOneSave) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->hub_.control_number(NUMBER_MAX_RANGE, 10.0f);
  this->hub_.control_number(NUMBER_HOLD_SENSITIVITY, 3.0f);
  EXPECT_FLOAT_EQ(this->max_range_.state, 10.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRange 1.00 10.00");
  this->expect_and_answer_("setSensitivity 3 5");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
}

TEST_F(SEN0609SequencerTest, WriteWaitsBeforeStarting) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  EXPECT_EQ(this->next_command_(500), "");
  EXPECT_EQ(this->next_command_(), "sensorStop");
}

TEST_F(SEN0609SequencerTest, NotStoppedStopsAndRetries) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  ASSERT_EQ(this->next_command_(), "setRange 1.00 12.00");
  // The module was started behind our back
  this->feed_("setRange 1.00 12.00\r\nsensor is not stopped\r\nDFRobot:/>");
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRange 1.00 12.00");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("sensorStart");
}

TEST_F(SEN0609SequencerTest, MalformedAnswerIsRetried) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6\r\n");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  EXPECT_FLOAT_EQ(this->max_range_.state, 12.0f);
}

TEST_F(SEN0609SequencerTest, ErrorAnswerIsRetriedThenSkipped) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "getRange");
    this->feed_("getRange\r\nError\r\nDFRobot:/>");
  }
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
  this->expect_and_answer_("sensorStart");
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_FALSE(this->min_range_.has_state());
}

TEST_F(SEN0609SequencerTest, LateAnswerIsNotTakenForNextCommand) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  for (int i = 0; i < 3; i++)
    ASSERT_EQ(this->next_command_(), "getRange");
  ASSERT_EQ(this->next_command_(), "getSensitivity");
  // The answer to the last getRange arrives after it timed out
  this->feed_("getRange\r\nResponse 0.6 12.0\r\nDone\r\nDFRobot:/>");
  this->answer_("getSensitivity", "Response 7 5\r\n");
  EXPECT_FLOAT_EQ(this->hold_sensitivity_.state, 7.0f);
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
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

TEST_F(SEN0609SequencerTest, PairedValueUnknownSkipsWriteAndRereads) {
  // No boot reads, so the trigger sensitivity paired with hold sensitivity is unknown
  this->hub_.control_number(NUMBER_HOLD_SENSITIVITY, 3.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("sensorStart");
  EXPECT_FLOAT_EQ(this->hold_sensitivity_.state, 7.0f);
  EXPECT_EQ(this->next_command_(), "");
}

TEST_F(SEN0609SequencerTest, ChangeBeforeBootReadWaitsForPairedValue) {
  this->hub_.setup();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  EXPECT_FLOAT_EQ(this->min_range_.state, 1.0f);
  EXPECT_FLOAT_EQ(this->max_range_.state, 12.0f);
  this->expect_and_answer_("setRange 1.00 12.00");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
}

TEST_F(SEN0609SequencerTest, ChangeDuringWriteIsWrittenAgain) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  ASSERT_EQ(this->next_command_(), "setRange 1.00 12.00");
  this->hub_.control_number(NUMBER_MIN_RANGE, 2.0f);
  this->answer_("setRange 1.00 12.00");
  this->expect_and_answer_("setRange 2.00 12.00");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
}

TEST_F(SEN0609SequencerTest, ChangeDuringReadIsKept) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  ASSERT_EQ(this->next_command_(), "getRange");
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->answer_("getRange", "Response 0.6 12.0\r\n");
  EXPECT_FLOAT_EQ(this->min_range_.state, 1.0f);
  this->expect_and_answer_("setRange 1.00 12.00");
}

TEST_F(SEN0609SequencerTest, FailedWriteShowsModuleValueAgain) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "setRange 1.00 12.00");
    this->feed_("setRange 1.00 12.00\r\nError\r\nDFRobot:/>");
  }
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("sensorStart");
  EXPECT_FLOAT_EQ(this->min_range_.state, 0.6f);
  EXPECT_TRUE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, AlreadyStoppedCountsAsStopped) {
  this->hub_.setup();
  ASSERT_EQ(this->next_command_(), "sensorStop");
  this->feed_("sensorStop\r\nsensor stopped already\r\nDFRobot:/>");
  EXPECT_EQ(this->next_command_(), "setRunApp 0");
}

class SEN0609SettingsTest : public SEN0609Test {
 protected:
  void SetUp() override {
    SEN0609Test::SetUp();
    this->run_led_.set_restore_mode(switch_::SWITCH_ALWAYS_ON);
    this->run_led_.set_inverted(true);
    this->out_led_.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
    this->out_pin_level_.traits.set_options({"low", "high"});
    this->hub_.set_switch(SWITCH_MICRO_MOTION, &this->micro_motion_);
    this->hub_.set_switch(SWITCH_RUN_LED, &this->run_led_);
    this->hub_.set_switch(SWITCH_OUT_LED, &this->out_led_);
    this->hub_.set_out_pin_level_select(&this->out_pin_level_);
    this->hub_.set_hardware_version_text_sensor(&this->hardware_version_);
  }

  void boot_() {
    this->hub_.setup();
    this->expect_and_answer_("sensorStop");
    this->expect_and_answer_("setRunApp 0");
    this->expect_and_answer_("setUartOutput 1 1 1 1.0");
    // Inverted and on: the LED is off, which is mode 1
    this->expect_and_answer_("setLedMode 1 1");
    this->expect_and_answer_("setLedMode 2 1");
    this->expect_and_answer_("getMicroMotion", "Response 1\r\n");
    this->expect_and_answer_("getGpioMode 1", "Response 1 1\r\n");
    this->expect_and_answer_("getHWV", "HardwareVersion:V2.0\r\n");
    this->expect_and_answer_("sensorStart");
  }

  TestSwitch micro_motion_;
  TestSwitch run_led_;
  TestSwitch out_led_;
  TestSelect out_pin_level_;
  text_sensor::TextSensor hardware_version_;
};

TEST_F(SEN0609SettingsTest, BootWritesLedsAndReadsSettings) {
  this->boot_();
  EXPECT_TRUE(this->run_led_.state);
  EXPECT_FALSE(this->out_led_.state);
  EXPECT_TRUE(this->micro_motion_.state);
  EXPECT_EQ(this->out_pin_level_.active_index(), optional<size_t>(1));
  EXPECT_EQ(this->hardware_version_.state, "V2.0");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SettingsTest, SwitchAndSelectChangesAreWrittenAndSaved) {
  this->boot_();
  this->hub_.control_switch(SWITCH_MICRO_MOTION, false);
  this->hub_.control_out_pin_level(0);
  this->hub_.control_switch(SWITCH_OUT_LED, true);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setMicroMotion 0");
  this->expect_and_answer_("setGpioMode 1 0");
  this->expect_and_answer_("setLedMode 2 0");
  // Answers that refuse a command whose effect is already in place count as done
  ASSERT_EQ(this->next_command_(), "saveConfig");
  this->feed_("saveConfig\r\nno parameter has been changed\r\nDFRobot:/>");
  ASSERT_EQ(this->next_command_(), "sensorStart");
  this->feed_("sensorStart\r\nsensor started already\r\nDFRobot:/>");
  EXPECT_EQ(this->next_command_(), "");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

}  // namespace esphome::dfrobot_sen0609::testing
