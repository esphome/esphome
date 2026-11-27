#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/mq_gas_sensors/mq_gas_sensor.h"

namespace esphome::mq_gas_sensors::testing {

/// Voltage source whose reading the test sets directly.
class CorrectionSampler : public voltage_sampler::VoltageSampler {
 public:
  float sample() override { return this->value; }
  float value{1.0f};
};

class CorrectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.set_a(976.97f);
    this->sensor_.set_b(-0.688f);
    this->sensor_.set_regression_method(RegressionMethod::REGRESSION_METHOD_EXPONENTIAL);
    this->sensor_.set_r0(10.0f);
    this->sensor_.set_max_ppm(10000.0f);
    this->sensor_.set_samples(1);
    this->sensor_.set_correction_sensor(&this->correction_);
  }

  void enable_compensation_() {
    this->sensor_.set_correction_mode(CorrectionMode::CORRECTION_MODE_MQDATASCIENCE);
    this->sensor_.set_tc_coefficients(0.8559f, -0.0611f, 0.1673f, 0.8201f, -0.0606f, 0.1492f);
    this->sensor_.set_temperature_source(&this->temperature_);
    this->sensor_.set_humidity_source(&this->humidity_);
  }

  CorrectionSampler sampler_;
  MQGasSensor sensor_{&sampler_};
  sensor::Sensor correction_;
  sensor::Sensor temperature_;
  sensor::Sensor humidity_;
};

TEST_F(CorrectionTest, AFaultPublishesTheCorrectionAsUnknown) {
  this->sensor_.setup();
  this->sensor_.update();
  ASSERT_FLOAT_EQ(1.0f, this->correction_.state);

  this->sampler_.value = NAN;
  this->sensor_.update();

  EXPECT_TRUE(std::isnan(this->correction_.state));
}

TEST_F(CorrectionTest, ASourceWithoutAReadingIsNotUsedAndIsReported) {
  this->enable_compensation_();
  this->sensor_.setup();

  this->sensor_.update();
  EXPECT_FLOAT_EQ(1.0f, this->correction_.state);
  EXPECT_TRUE(this->sensor_.status_has_warning());

  this->temperature_.publish_state(30.0f);
  this->humidity_.publish_state(50.0f);
  this->sensor_.update();
  EXPECT_TRUE(std::isfinite(this->correction_.state));
  EXPECT_NE(1.0f, this->correction_.state);
  EXPECT_FALSE(this->sensor_.status_has_warning());

  // The source keeps `has_state()`, but its value is no longer a measurement.
  this->temperature_.publish_state(NAN);
  this->sensor_.update();
  EXPECT_FLOAT_EQ(1.0f, this->correction_.state);
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(CorrectionTest, ACalibrationWithoutAmbientReadingsStoresNoReference) {
  this->enable_compensation_();
  this->sensor_.set_calibration(true, 70.0f, 0, 0, 2, false);
  this->sensor_.set_sample_interval(0);
  this->sensor_.setup();

  this->sensor_.start_calibration();
  for (int i = 0; i < 4; i++)
    this->sensor_.loop();
  this->temperature_.publish_state(20.0f);
  this->humidity_.publish_state(65.0f);
  this->sensor_.update();

  // The configured R0 of 10 kOhm is still in use: RS = 40 kOhm against it.
  sensor::Sensor ratio;
  this->sensor_.set_ratio_sensor(&ratio);
  this->sensor_.update();
  EXPECT_FLOAT_EQ(4.0f, ratio.state);
}

TEST_F(CorrectionTest, ACalibrationStartedByADiagnosticKeepsThePpmUnknown) {
  this->sensor_.set_calibration(true, 70.0f, 0, 0, 2, false);
  this->sensor_.setup();
  bool started = false;
  this->correction_.add_on_state_callback([this, &started](float) {
    if (!started) {
      started = true;
      this->sensor_.start_calibration();
    }
  });

  this->sensor_.update();

  EXPECT_TRUE(started);
  EXPECT_TRUE(std::isnan(this->sensor_.state));
}

}  // namespace esphome::mq_gas_sensors::testing
