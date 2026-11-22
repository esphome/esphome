#include <gtest/gtest.h>

#include "esphome/components/mq_gas_sensors/mq_gas_sensor.h"

namespace esphome::mq_gas_sensors::testing {

/// Voltage source whose reading the test sets directly.
class FakeSampler : public voltage_sampler::VoltageSampler {
 public:
  float sample() override { return this->value; }
  float value{1.0f};
};

class DiagnosticsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.set_a(976.97f);
    this->sensor_.set_b(-0.688f);
    this->sensor_.set_regression_method(RegressionMethod::REGRESSION_EXPONENTIAL);
    this->sensor_.set_r0(10.0f);
    this->sensor_.set_rl(10.0f);
    this->sensor_.set_vcc(5.0f);
    this->sensor_.set_samples(1);
    this->sensor_.set_voltage_multiplier(1.0f);
    this->sensor_.set_min_ppm(0.0f);
    this->sensor_.set_max_ppm(10000.0f);
    this->sensor_.set_voltage_sensor(&this->voltage_sensor_);
    this->sensor_.set_rs_sensor(&this->rs_sensor_);
    this->sensor_.set_ratio_sensor(&this->ratio_sensor_);
  }

  FakeSampler sampler_;
  MQGasSensor sensor_{&sampler_};
  sensor::Sensor voltage_sensor_;
  sensor::Sensor rs_sensor_;
  sensor::Sensor ratio_sensor_;
};

TEST_F(DiagnosticsTest, ValidReadingPublishesTheChain) {
  this->sampler_.value = 1.0f;
  this->sensor_.update();

  // RS = VCC * RL / V - RL = 40 kOhm, ratio = RS / R0 = 4.
  EXPECT_FLOAT_EQ(1.0f, this->voltage_sensor_.state);
  EXPECT_FLOAT_EQ(40.0f, this->rs_sensor_.state);
  EXPECT_FLOAT_EQ(4.0f, this->ratio_sensor_.state);
  EXPECT_TRUE(std::isfinite(this->sensor_.state));
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

TEST_F(DiagnosticsTest, MissingSignalPublishesUnknownDiagnostics) {
  this->sampler_.value = NAN;
  this->sensor_.update();

  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(std::isnan(this->voltage_sensor_.state));
  EXPECT_TRUE(std::isnan(this->rs_sensor_.state));
  EXPECT_TRUE(std::isnan(this->ratio_sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(DiagnosticsTest, RecoveryClearsTheFaultAndPublishesAgain) {
  this->sampler_.value = NAN;
  this->sensor_.update();
  ASSERT_TRUE(this->sensor_.status_has_warning());

  this->sampler_.value = 1.0f;
  this->sensor_.update();

  EXPECT_FLOAT_EQ(1.0f, this->voltage_sensor_.state);
  EXPECT_FLOAT_EQ(40.0f, this->rs_sensor_.state);
  EXPECT_FLOAT_EQ(4.0f, this->ratio_sensor_.state);
  EXPECT_TRUE(std::isfinite(this->sensor_.state));
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

TEST_F(DiagnosticsTest, SupplyRailPublishesTheCeilingAndRecovers) {
  // RS has collapsed to 0: the datasheet curve reports the top of its range.
  this->sampler_.value = 5.0f;
  this->sensor_.update();
  EXPECT_FLOAT_EQ(10000.0f, this->sensor_.state);
  EXPECT_FLOAT_EQ(0.0f, this->rs_sensor_.state);
  EXPECT_FLOAT_EQ(0.0f, this->ratio_sensor_.state);
  EXPECT_FALSE(this->sensor_.status_has_warning());

  // Far above the supply is a wiring fault, not a reading.
  this->sampler_.value = 5.5f;
  this->sensor_.update();
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());

  // A normal sample again publishes the chain and clears the fault.
  this->sampler_.value = 1.0f;
  this->sensor_.update();
  EXPECT_FLOAT_EQ(40.0f, this->rs_sensor_.state);
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

}  // namespace esphome::mq_gas_sensors::testing
