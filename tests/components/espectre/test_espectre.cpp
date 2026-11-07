#include <cmath>
#include <vector>
#include "gtest/gtest.h"
#include "esphome/components/espectre/espectre.h"

#ifdef ESPECTRE_SDK_TEST_DOUBLE

namespace esphome::espectre::testing {

class TestNumber : public number::Number {
 protected:
  void control(float value) override {}
};

class TestSelect : public select::Select {
 protected:
  void control(size_t index) override {}
};

class TestSwitch : public switch_::Switch {
 protected:
  void write_state(bool state) override {}
};

static constexpr const char *const DETECTION_ALGORITHM_OPTIONS[] = {"lightweight", "high_accuracy"};
static constexpr const char *const CSI_SAMPLE_SPACING_OPTIONS[] = {"half_slot", "distinct"};

class ESPectreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Each test starts without the controls an earlier test saved.
    if (global_preferences != nullptr)
      global_preferences->reset();
    component_.set_motion_binary_sensor(&motion_);
    component_.set_calibrating_binary_sensor(&calibrating_);
    component_.set_movement_sensor(&movement_);
  }

  void make_ready_(bool motion = true) {
    sdk_->snapshot_.ready_to_publish = true;
    sdk_->snapshot_.motion_state = motion ? ::espectre::MotionState::MOTION : ::espectre::MotionState::IDLE;
    sdk_->snapshot_.movement_metric = 0.8f;
    component_.loop();
  }

  void add_controls_() {
    component_.set_threshold_number(&threshold_);
    component_.set_motion_on_hits_number(&motion_on_hits_);
    component_.set_motion_off_hits_number(&motion_off_hits_);
    detection_algorithm_.traits.set_options_static(DETECTION_ALGORITHM_OPTIONS, 2);
    component_.set_detection_algorithm_select(&detection_algorithm_);
    csi_sample_spacing_.traits.set_options_static(CSI_SAMPLE_SPACING_OPTIONS, 2);
    component_.set_csi_sample_spacing_select(&csi_sample_spacing_);
    // The YAML default; the Switch class itself defaults to off.
    sensing_.set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON);
    component_.set_sensing_switch(&sensing_);
  }

  ESPectreComponent component_;
  ::espectre::RuntimeFrontendController *sdk_{::espectre::RuntimeFrontendController::instance};
  binary_sensor::BinarySensor motion_;
  binary_sensor::BinarySensor calibrating_;
  sensor::Sensor movement_;
  TestNumber threshold_;
  TestNumber motion_on_hits_;
  TestNumber motion_off_hits_;
  TestSelect detection_algorithm_;
  TestSelect csi_sample_spacing_;
  TestSwitch sensing_;
};

TEST_F(ESPectreTest, StartupDoesNotPublishFalseMotion) {
  component_.setup();
  component_.loop();
  EXPECT_FALSE(motion_.has_state());
  EXPECT_FALSE(movement_.has_state());
  EXPECT_TRUE(calibrating_.has_state());
}

TEST_F(ESPectreTest, SetupHandsScanResultsAndPersistenceToEspHome) {
  component_.setup();
  EXPECT_TRUE(sdk_->config_at_setup_.wifi_scan_results_managed_externally);
  EXPECT_FALSE(sdk_->config_at_setup_.persist_runtime_overrides);
  // Only the detection algorithm select needs runtime detector selection.
  EXPECT_FALSE(sdk_->config_at_setup_.runtime_detector_selection_enabled);
}

TEST_F(ESPectreTest, DetectionAlgorithmSelectEnablesDetectorSelection) {
  add_controls_();
  component_.setup();
  EXPECT_TRUE(sdk_->config_at_setup_.runtime_detector_selection_enabled);
}

TEST_F(ESPectreTest, ReadinessLossAndRecovery) {
  component_.setup();
  std::vector<optional<bool>> published_states;
  motion_.add_full_state_callback(
      [&published_states](optional<bool>, optional<bool> current) { published_states.push_back(current); });
  for (bool motion : {false, true}) {
    published_states.clear();
    make_ready_(motion);
    ASSERT_TRUE(motion_.has_state());
    EXPECT_EQ(motion_.state, motion);
    EXPECT_FLOAT_EQ(movement_.state, 0.8f);

    sdk_->snapshot_.ready_to_publish = false;
    sdk_->snapshot_.calibrating = true;
    component_.loop();
    EXPECT_FALSE(motion_.has_state());
    EXPECT_TRUE(std::isnan(movement_.state));
    EXPECT_TRUE(calibrating_.state);

    sdk_->snapshot_.calibrating = false;
    make_ready_(motion);
    EXPECT_TRUE(motion_.has_state());
    EXPECT_EQ(motion_.state, motion);
    EXPECT_FLOAT_EQ(movement_.state, 0.8f);
    EXPECT_EQ(published_states, (std::vector<optional<bool>>{motion, nullopt, motion}));
  }
}

TEST_F(ESPectreTest, TelemetryUsesFinalReadiness) {
  component_.setup();
  make_ready_();
  std::vector<float> published_states;
  movement_.add_on_state_callback([&published_states](float state) { published_states.push_back(state); });
  sdk_->loop_hook = [this]() {
    sdk_->listener->on_live_telemetry(sdk_->snapshot_);
    sdk_->snapshot_.movement_metric = 0.9f;
    sdk_->snapshot_.ready_to_publish = false;
  };
  component_.loop();
  EXPECT_FALSE(motion_.has_state());
  EXPECT_TRUE(std::isnan(movement_.state));
  ASSERT_EQ(published_states.size(), 1u);
  EXPECT_TRUE(std::isnan(published_states.front()));
}

TEST_F(ESPectreTest, EntityCallbackQueuesRecalibration) {
  component_.setup();
  movement_.add_on_state_callback([this](float) { component_.recalibrate(); });
  make_ready_();
  EXPECT_EQ(sdk_->recalibration_calls, 0u);
  component_.loop();
  EXPECT_EQ(sdk_->recalibration_calls, 1u);
}

TEST_F(ESPectreTest, CalibrationFailureWarnsOnlyBeforeFirstSuccess) {
  component_.setup();
  sdk_->listener->on_calibration_finished(sdk_->snapshot_, false);
  EXPECT_TRUE(component_.status_has_warning());
  EXPECT_FALSE(component_.is_failed());
  sdk_->listener->on_calibration_finished(sdk_->snapshot_, true);
  EXPECT_FALSE(component_.status_has_warning());
  sdk_->listener->on_calibration_finished(sdk_->snapshot_, false);
  EXPECT_FALSE(component_.status_has_warning());
}

TEST_F(ESPectreTest, RuntimeFaultStopsAndWaitsToRestart) {
  component_.setup();
  make_ready_();
  sdk_->listener->on_runtime_fault("test fault");
  component_.loop();
  EXPECT_FALSE(component_.is_failed());
  EXPECT_TRUE(component_.status_has_error());
  EXPECT_TRUE(sdk_->shutdown_called);
  EXPECT_FALSE(motion_.has_state());
  EXPECT_TRUE(std::isnan(movement_.state));

  component_.loop();
  EXPECT_EQ(sdk_->setup_calls, 1u);
}

TEST_F(ESPectreTest, FailedSetupStopsAndWaitsToRestart) {
  sdk_->setup_result = false;
  component_.setup();
  EXPECT_FALSE(component_.is_failed());
  EXPECT_TRUE(component_.status_has_error());
  EXPECT_TRUE(sdk_->shutdown_called);
}

TEST_F(ESPectreTest, TrafficModeRequestAppliesInLoop) {
  component_.setup();
  component_.request_traffic_generator_mode(::espectre::TrafficGeneratorMode::EXTERNAL_HOST);
  EXPECT_EQ(sdk_->config_.traffic_generator_mode, ::espectre::TrafficGeneratorMode::PING);
  component_.loop();
  EXPECT_EQ(sdk_->config_.traffic_generator_mode, ::espectre::TrafficGeneratorMode::EXTERNAL_HOST);
}

TEST_F(ESPectreTest, TrafficModeRequestByName) {
  component_.setup();
  component_.request_traffic_generator_mode("unknown");
  component_.loop();
  EXPECT_EQ(sdk_->config_.traffic_generator_mode, ::espectre::TrafficGeneratorMode::PING);
  component_.request_traffic_generator_mode("dns_tcp");
  component_.loop();
  EXPECT_EQ(sdk_->config_.traffic_generator_mode, ::espectre::TrafficGeneratorMode::DNS_TCP);
}

TEST_F(ESPectreTest, ShutdownInvalidatesEntities) {
  component_.setup();
  make_ready_();
  component_.on_shutdown();
  EXPECT_TRUE(sdk_->shutdown_called);
  EXPECT_FALSE(motion_.has_state());
  EXPECT_FALSE(calibrating_.has_state());
  EXPECT_TRUE(std::isnan(movement_.state));
}

TEST_F(ESPectreTest, ControlRequestsApplyInLoop) {
  add_controls_();
  component_.setup();
  EXPECT_FLOAT_EQ(motion_on_hits_.state, 4.0f);
  EXPECT_STREQ(detection_algorithm_.current_option().c_str(), "lightweight");
  EXPECT_STREQ(csi_sample_spacing_.current_option().c_str(), "half_slot");
  EXPECT_TRUE(sensing_.state);

  component_.request_motion_off_hits(7.0f);
  component_.request_detection_algorithm("high_accuracy");
  component_.request_csi_sample_spacing("distinct");
  component_.request_sensing(false);
  EXPECT_TRUE(sdk_->services_armed());
  component_.loop();
  EXPECT_EQ(sdk_->config_.motion_on_hits, 4);
  EXPECT_EQ(sdk_->config_.motion_off_hits, 7);
  EXPECT_FLOAT_EQ(motion_off_hits_.state, 7.0f);
  EXPECT_STREQ(detection_algorithm_.current_option().c_str(), "high_accuracy");
  EXPECT_EQ(sdk_->config_.csi_sample_spacing, ::espectre::TemporalCsiSpacing::DISTINCT);
  EXPECT_STREQ(csi_sample_spacing_.current_option().c_str(), "distinct");
  // A detector change applies that detector's default threshold.
  EXPECT_FLOAT_EQ(threshold_.state,
                  ::espectre::runtime_default_threshold(::espectre::DetectionAlgorithm::HIGH_ACCURACY));
  EXPECT_FALSE(sdk_->services_armed());
  EXPECT_FALSE(sensing_.state);
}

TEST_F(ESPectreTest, ControlChangeRepublishesOnlyItsEntity) {
  add_controls_();
  component_.setup();
  int spacing_publishes = 0;
  int on_hits_publishes = 0;
  csi_sample_spacing_.add_on_state_callback([&spacing_publishes](size_t) { spacing_publishes++; });
  motion_on_hits_.add_on_state_callback([&on_hits_publishes](float) { on_hits_publishes++; });
  component_.request_motion_off_hits(7.0f);
  component_.request_detection_algorithm("high_accuracy");
  component_.loop();
  EXPECT_FLOAT_EQ(motion_off_hits_.state, 7.0f);
  EXPECT_STREQ(detection_algorithm_.current_option().c_str(), "high_accuracy");
  // Both motion hits numbers republish together, since the SDK sets them as a pair.
  EXPECT_EQ(on_hits_publishes, 1);
  EXPECT_EQ(spacing_publishes, 0);
}

TEST_F(ESPectreTest, ControlRequestsWithoutEntities) {
  component_.setup();
  component_.request_threshold(0.4f);
  component_.request_sensing(false);
  // Without the select, the SDK does not allow detector changes, so the request is rejected.
  component_.request_detection_algorithm("high_accuracy");
  component_.loop();
  EXPECT_FLOAT_EQ(sdk_->snapshot_.threshold, 0.4f);
  EXPECT_FALSE(sdk_->services_armed());
  EXPECT_EQ(sdk_->config_.detection_algorithm, ::espectre::DetectionAlgorithm::LIGHTWEIGHT);
}

TEST_F(ESPectreTest, ThresholdPublishesRuntimeValue) {
  add_controls_();
  component_.setup();
  component_.request_threshold(0.4f);
  component_.loop();
  EXPECT_FLOAT_EQ(threshold_.state, 0.4f);

  threshold_.publish_state(2.0f);
  component_.request_threshold(2.0f);
  component_.loop();
  EXPECT_FLOAT_EQ(threshold_.state, 0.4f);

  sdk_->loop_hook = [this]() { sdk_->snapshot_.threshold = 0.3f; };
  component_.loop();
  EXPECT_FLOAT_EQ(threshold_.state, 0.3f);
}

TEST_F(ESPectreTest, SavedControlsRestoreUntilYamlChanges) {
  add_controls_();
  component_.setup();
  component_.request_motion_on_hits(9.0f);
  component_.request_detection_algorithm("high_accuracy");
  component_.request_csi_sample_spacing("distinct");
  component_.loop();

  ESPectreComponent same_yaml;
  same_yaml.set_motion_on_hits_number(&motion_on_hits_);
  same_yaml.set_detection_algorithm_select(&detection_algorithm_);
  same_yaml.set_csi_sample_spacing_select(&csi_sample_spacing_);
  same_yaml.setup();
  const auto &restored = ::espectre::RuntimeFrontendController::instance->config_at_setup_;
  EXPECT_EQ(restored.motion_on_hits, 9);
  EXPECT_EQ(restored.detection_algorithm, ::espectre::DetectionAlgorithm::HIGH_ACCURACY);
  EXPECT_EQ(restored.csi_sample_spacing, ::espectre::TemporalCsiSpacing::DISTINCT);

  ESPectreComponent changed_yaml;
  changed_yaml.set_motion_on_hits(2);
  changed_yaml.set_motion_on_hits_number(&motion_on_hits_);
  changed_yaml.setup();
  EXPECT_EQ(::espectre::RuntimeFrontendController::instance->config_at_setup_.motion_on_hits, 2);
}

TEST_F(ESPectreTest, SensingRestoreModeAppliesBeforeStartup) {
  sensing_.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  component_.set_sensing_switch(&sensing_);
  component_.setup();
  EXPECT_FALSE(sdk_->services_armed());
  EXPECT_FALSE(sensing_.state);
}

}  // namespace esphome::espectre::testing

#endif  // ESPECTRE_SDK_TEST_DOUBLE
