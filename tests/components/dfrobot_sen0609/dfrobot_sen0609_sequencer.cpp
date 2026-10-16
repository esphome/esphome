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

TEST_F(SEN0609SequencerTest, NonFiniteAnswerIsRetried) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response nan 12.0\r\n");
  this->expect_and_answer_("getRange", "Response 0.6 inf\r\n");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  EXPECT_FLOAT_EQ(this->min_range_.state, 0.6f);
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

TEST_F(SEN0609SequencerTest, LateVersionLineIsStillUsed) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  for (int i = 0; i < 3; i++)
    ASSERT_EQ(this->next_command_(), "getSWV");
  ASSERT_EQ(this->next_command_(), "sensorStart");
  // The answer to the last getSWV arrives after it timed out; the version line names its value
  this->feed_("getSWV\r\nSoftwareVersion:V1.0.4\r\nDone\r\nDFRobot:/>");
  this->answer_("sensorStart");
  EXPECT_EQ(this->version_.state, "V1.0.4");
}

TEST_F(SEN0609SequencerTest, MissingVersionLineIsRetried) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("getSWV");
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
  EXPECT_EQ(this->version_.state, "V1.0.4");
  this->expect_and_answer_("sensorStart");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, FailedConfigurationIsRetriedLater) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "setRunApp 0");
    this->feed_("setRunApp 0\r\nError\r\nDFRobot:/>");
  }
  EXPECT_TRUE(this->hub_.status_has_warning());
  // The sensor is not started in an unknown mode
  EXPECT_EQ(this->next_command_(20000), "");
  ASSERT_EQ(this->next_command_(15000), "setRunApp 0");
  this->answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
  this->expect_and_answer_("sensorStart");
  EXPECT_FALSE(this->hub_.status_has_warning());
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

TEST_F(SEN0609SequencerTest, PairedValueUnknownIsReadBeforeWrite) {
  // No boot reads, so the trigger sensitivity paired with hold sensitivity is unknown
  this->hub_.control_number(NUMBER_HOLD_SENSITIVITY, 3.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  EXPECT_FLOAT_EQ(this->hold_sensitivity_.state, 3.0f);
  this->expect_and_answer_("setSensitivity 3 5");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
}

TEST_F(SEN0609SequencerTest, PairedValueReadFailureDropsWrite) {
  this->hub_.control_number(NUMBER_HOLD_SENSITIVITY, 3.0f);
  this->expect_and_answer_("sensorStop");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "getSensitivity");
    this->feed_("getSensitivity\r\nError\r\nDFRobot:/>");
  }
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
  EXPECT_TRUE(this->hub_.status_has_warning());
  // Whether the change reached the module is unknown
  EXPECT_TRUE(std::isnan(this->hold_sensitivity_.state));
}

TEST_F(SEN0609SequencerTest, ChangeAfterFailedBootReadIsWritten) {
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
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("setRange 1.00 12.00");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("sensorStart");
  EXPECT_FLOAT_EQ(this->min_range_.state, 1.0f);
  EXPECT_FALSE(this->hub_.status_has_warning());
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

TEST_F(SEN0609SequencerTest, ChangesDuringSequenceShareOneSave) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  ASSERT_EQ(this->next_command_(), "setRange 1.00 12.00");
  this->hub_.control_number(NUMBER_MAX_RANGE, 10.0f);
  this->answer_("setRange 1.00 12.00");
  this->expect_and_answer_("setRange 1.00 10.00");
  // The save waits a second after the last change
  EXPECT_EQ(this->next_command_(500), "");
  this->hub_.control_number(NUMBER_HOLD_SENSITIVITY, 3.0f);
  this->expect_and_answer_("setSensitivity 3 5");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
}

TEST_F(SEN0609SequencerTest, ReadAfterFailedWriteKeepsNewerPartnerChange) {
  this->boot_();
  this->hub_.control_number(NUMBER_HOLD_SENSITIVITY, 9.0f);
  this->expect_and_answer_("sensorStop");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "setSensitivity 9 5");
    this->feed_("setSensitivity 9 5\r\nError\r\nDFRobot:/>");
  }
  ASSERT_EQ(this->next_command_(), "getSensitivity");
  this->hub_.control_number(NUMBER_TRIGGER_SENSITIVITY, 4.0f);
  this->answer_("getSensitivity", "Response 7 5\r\n");
  // The rejected hold sensitivity is put back, the newer trigger sensitivity is kept
  EXPECT_FLOAT_EQ(this->hold_sensitivity_.state, 7.0f);
  this->expect_and_answer_("setSensitivity 7 4");
}

TEST_F(SEN0609SequencerTest, AutomationDuringReadKeepsItsChange) {
  bool changed = false;
  this->min_range_.add_on_state_callback([this, &changed](float) {
    if (!changed) {
      changed = true;
      this->hub_.control_number(NUMBER_MAX_RANGE, 8.0f);
    }
  });
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  EXPECT_FLOAT_EQ(this->max_range_.state, 8.0f);
  this->expect_and_answer_("setRange 0.60 8.00");
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

TEST_F(SEN0609SequencerTest, QueryKeepsGapAfterAnswer) {
  this->boot_();
  this->hub_.query_params();
  this->run_();
  EXPECT_EQ(this->uart_.tx, "");
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
  this->expect_and_answer_("sensorStart");
}

TEST_F(SEN0609SequencerTest, AlreadyStoppedCountsAsStopped) {
  this->hub_.setup();
  ASSERT_EQ(this->next_command_(), "sensorStop");
  this->feed_("sensorStop\r\nsensor stopped already\r\nDFRobot:/>");
  EXPECT_EQ(this->next_command_(), "setRunApp 0");
}

TEST_F(SEN0609SequencerTest, NotStoppedStopsAgainBeforeRetry) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  ASSERT_EQ(this->next_command_(), "setRunApp 0");
  // The sensor was started again behind the hub's back
  this->feed_("setRunApp 0\r\nsensor is not stopped\r\nDFRobot:/>");
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  EXPECT_EQ(this->next_command_(), "setUartOutput 1 1 1 1.0");
}

TEST_F(SEN0609SequencerTest, AlreadyStartedCountsAsStarted) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
  // As left by an earlier sequence that failed
  this->hub_.status_set_warning();
  ASSERT_EQ(this->next_command_(), "sensorStart");
  this->feed_("sensorStart\r\nsensor started already\r\nDFRobot:/>");
  EXPECT_EQ(this->next_command_(), "");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, PresenceFramesKeepConfiguration) {
  this->boot_();
  for (int i = 0; i < 15; i++) {
    this->feed_("$DFHPD,0, , , *\r\n");
    this->run_(1000);
  }
  EXPECT_EQ(this->uart_.tx, "");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, MissingPresenceFramesReapplyConfiguration) {
  this->boot_();
  this->feed_("$DFHPD,1, , , *\r\n");
  EXPECT_TRUE(this->has_target_.state);
  EXPECT_EQ(this->next_command_(9000), "");
  // The module restarted on its own and stopped sending frames
  ASSERT_EQ(this->next_command_(), "sensorStop");
  EXPECT_FALSE(this->has_target_.has_state());
  EXPECT_TRUE(this->hub_.status_has_warning());
  this->answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
  this->expect_and_answer_("sensorStart");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, ChangeDuringBackOffKeepsBackOff) {
  this->hub_.setup();
  for (int i = 0; i < 3; i++)
    ASSERT_EQ(this->next_command_(), "sensorStop");
  this->run_(2100);  // the last attempt times out
  EXPECT_TRUE(this->hub_.status_has_warning());
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  EXPECT_EQ(this->next_command_(20000), "");
  EXPECT_EQ(this->next_command_(15000), "sensorStop");
}

TEST_F(SEN0609SequencerTest, FailedSaveIsRetriedLater) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRange 1.00 12.00");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "saveConfig");
    this->feed_("saveConfig\r\nError\r\nDFRobot:/>");
  }
  // The sensor runs again meanwhile
  this->expect_and_answer_("sensorStart");
  EXPECT_TRUE(this->hub_.status_has_warning());
  this->feed_frames_(28);
  EXPECT_EQ(this->uart_.tx, "");
  this->feed_frames_(4);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SequencerTest, ErrorMentioningNoParameterIsNotASave) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRange 1.00 12.00");
  ASSERT_EQ(this->next_command_(), "saveConfig");
  this->feed_("saveConfig\r\nError: no parameter saved\r\nDFRobot:/>");
  EXPECT_EQ(this->next_command_(), "saveConfig");
}

TEST_F(SEN0609SequencerTest, MinRangeAboveMaxRangeIsRefused) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 13.0f);
  EXPECT_FLOAT_EQ(this->min_range_.state, 0.6f);
  this->hub_.control_number(NUMBER_MAX_RANGE, 0.5f);
  EXPECT_FLOAT_EQ(this->max_range_.state, 12.0f);
  EXPECT_EQ(this->next_command_(3000), "");
}

TEST_F(SEN0609SequencerTest, TriggerRangeOutsideRangeIsRefused) {
  TestNumber trigger_range;
  this->hub_.set_number(NUMBER_TRIGGER_RANGE, &trigger_range);
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 12.0\r\n");
  this->expect_and_answer_("getTrigRange", "Response 6.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 7 5\r\n");
  this->expect_and_answer_("getSWV", "SoftwareVersion:V1.0.4\r\n");
  this->expect_and_answer_("sensorStart");
  this->hub_.control_number(NUMBER_TRIGGER_RANGE, 13.0f);
  EXPECT_FLOAT_EQ(trigger_range.state, 6.0f);
  // Lowering the maximum below the trigger range is refused too
  this->hub_.control_number(NUMBER_MAX_RANGE, 5.0f);
  EXPECT_FLOAT_EQ(this->max_range_.state, 12.0f);
  this->hub_.control_number(NUMBER_MIN_RANGE, 7.0f);
  EXPECT_FLOAT_EQ(this->min_range_.state, 0.6f);
  EXPECT_EQ(this->next_command_(3000), "");
  this->hub_.control_number(NUMBER_TRIGGER_RANGE, 8.0f);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setTrigRange 8.00");
}

TEST_F(SEN0609SequencerTest, RestartReappliesConfiguration) {
  this->boot_();
  this->hub_.restart();
  ASSERT_EQ(this->next_command_(), "resetSystem");
  // resetSystem has no answer; the hub waits for the reboot
  EXPECT_EQ(this->next_command_(1000), "");
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
}

TEST_F(SEN0609SequencerTest, RestartDuringSequenceSavesWrittenChange) {
  this->boot_();
  this->hub_.control_number(NUMBER_MIN_RANGE, 1.0f);
  this->expect_and_answer_("sensorStop");
  ASSERT_EQ(this->next_command_(), "setRange 1.00 12.00");
  this->hub_.control_number(NUMBER_HOLD_SENSITIVITY, 3.0f);
  this->hub_.restart();
  this->answer_("setRange 1.00 12.00");
  this->expect_and_answer_("saveConfig");
  ASSERT_EQ(this->next_command_(), "resetSystem");
  EXPECT_EQ(this->next_command_(1000), "");
  // The change not sent before the reboot is written after it
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("setSensitivity 3 5");
  this->expect_and_answer_("saveConfig");
  this->expect_and_answer_("getRange", "Response 1.0 12.0\r\n");
  this->expect_and_answer_("getSensitivity", "Response 3 5\r\n");
  EXPECT_FLOAT_EQ(this->min_range_.state, 1.0f);
  EXPECT_FLOAT_EQ(this->hold_sensitivity_.state, 3.0f);
}

TEST_F(SEN0609SequencerTest, FactoryResetRereadsSettings) {
  this->boot_();
  this->hub_.factory_reset();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("resetCfg");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  this->expect_and_answer_("getRange", "Response 0.6 25.0\r\n");
  EXPECT_FLOAT_EQ(this->max_range_.state, 25.0f);
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

TEST_F(SEN0609SettingsTest, FailedLedWriteShowsAcceptedStateAgain) {
  this->boot_();
  this->hub_.control_switch(SWITCH_OUT_LED, true);
  EXPECT_TRUE(this->out_led_.state);
  this->expect_and_answer_("sensorStop");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "setLedMode 2 0");
    this->feed_("setLedMode 2 0\r\nError\r\nDFRobot:/>");
  }
  // The LEDs cannot be read back, so the state the module accepted at boot is shown again
  EXPECT_FALSE(this->out_led_.state);
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
  EXPECT_TRUE(this->hub_.status_has_warning());
}

TEST_F(SEN0609SettingsTest, FailedBootLedWriteIsRetriedLater) {
  this->hub_.setup();
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setRunApp 0");
  this->expect_and_answer_("setUartOutput 1 1 1 1.0");
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(this->next_command_(), "setLedMode 1 1");
    this->feed_("setLedMode 1 1\r\nError\r\nDFRobot:/>");
  }
  this->expect_and_answer_("setLedMode 2 1");
  this->expect_and_answer_("getMicroMotion", "Response 1\r\n");
  this->expect_and_answer_("getGpioMode 1", "Response 1 1\r\n");
  this->expect_and_answer_("getHWV", "HardwareVersion:V2.0\r\n");
  // The sensor runs without the LED state, which is written again later
  this->expect_and_answer_("sensorStart");
  EXPECT_TRUE(this->hub_.status_has_warning());
  this->feed_frames_(28);
  EXPECT_EQ(this->uart_.tx, "");
  this->feed_frames_(4);
  this->expect_and_answer_("sensorStop");
  this->expect_and_answer_("setLedMode 1 1");
  this->expect_and_answer_("sensorStart");
  EXPECT_EQ(this->next_command_(), "");
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
