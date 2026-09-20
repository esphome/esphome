#include <cmath>
#include <vector>
#include "gtest/gtest.h"
#include "esphome/components/espectre/espectre.h"

#ifdef ESPECTRE_SDK_TEST_DOUBLE

namespace esphome::espectre::testing {

class ESPectreTest : public ::testing::Test {
 protected:
  void SetUp() override {
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

  ESPectreComponent component_;
  ::espectre::RuntimeFrontendController *sdk_{::espectre::RuntimeFrontendController::instance};
  binary_sensor::BinarySensor motion_;
  binary_sensor::BinarySensor calibrating_;
  sensor::Sensor movement_;
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
    sdk_->listener->on_live_telemetry(0.9f, 0.5f);
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
  component_.request_traffic_generator_mode(::espectre::TrafficGeneratorMode::EXTERNAL);
  EXPECT_EQ(sdk_->config_.traffic_generator_mode, ::espectre::TrafficGeneratorMode::PING);
  component_.loop();
  EXPECT_EQ(sdk_->config_.traffic_generator_mode, ::espectre::TrafficGeneratorMode::EXTERNAL);
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

}  // namespace esphome::espectre::testing

#endif  // ESPECTRE_SDK_TEST_DOUBLE
