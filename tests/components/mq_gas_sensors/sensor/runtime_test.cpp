#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/mq_gas_sensors/mq_gas_sensor.h"

namespace esphome::mq_gas_sensors::testing {

/// Voltage source whose reading the test sets directly.
class RuntimeSampler : public voltage_sampler::VoltageSampler {
 public:
  float sample() override { return this->value; }
  float value{1.0f};
};

class RuntimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.set_a(976.97f);  // MQ-8 H2, exponential
    this->sensor_.set_b(-0.688f);
    this->sensor_.set_regression_method(RegressionMethod::REGRESSION_METHOD_EXPONENTIAL);
    this->sensor_.set_r0(10.0f);
    this->sensor_.set_rl(10.0f);
    this->sensor_.set_vcc(5.0f);
    this->sensor_.set_samples(1);
    this->sensor_.set_voltage_multiplier(1.0f);
    this->sensor_.set_min_ppm(0.0f);
    this->sensor_.set_max_ppm(10000.0f);
  }

  void read_(float voltage) {
    this->sampler_.value = voltage;
    this->sensor_.update();
  }

  /// A fault publishes unknown and keeps a status warning; a good reading ends both.
  void expect_fault_then_recovery_(float voltage) {
    this->read_(voltage);
    EXPECT_TRUE(std::isnan(this->sensor_.state));
    EXPECT_TRUE(this->sensor_.status_has_warning());

    this->read_(1.0f);
    EXPECT_TRUE(std::isfinite(this->sensor_.state));
    EXPECT_FALSE(this->sensor_.status_has_warning());
  }

  RuntimeSampler sampler_;
  MQGasSensor sensor_{&sampler_};
};

TEST_F(RuntimeTest, AReadingJustAboveTheSupplyIsTheTopOfTheRange) {
  this->read_(5.1f);

  EXPECT_FLOAT_EQ(10000.0f, this->sensor_.state);
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

TEST_F(RuntimeTest, AReadingFarAboveTheSupplyIsAFault) { this->expect_fault_then_recovery_(5.5f); }

TEST_F(RuntimeTest, AMissingSignalIsAFault) { this->expect_fault_then_recovery_(0.0f); }

TEST_F(RuntimeTest, ASamplerWithoutAReadingIsAFault) { this->expect_fault_then_recovery_(NAN); }

TEST_F(RuntimeTest, ACurveThatCannotBeEvaluatedKeepsTheWarning) {
  this->sensor_.set_regression_method(static_cast<RegressionMethod>(99));

  this->read_(1.0f);
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());

  // A second update must not clear it: the readings are still unknown.
  this->read_(1.0f);
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(RuntimeTest, ASaturatedCurveWithoutATopIsUnknown) {
  this->sensor_.set_b(0.688f);  // falls to zero as the ratio does

  this->read_(5.0f);

  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(RuntimeTest, AnOverRangeReadingIsClampedToTheCeiling) {
  this->read_(4.99f);

  EXPECT_FLOAT_EQ(10000.0f, this->sensor_.state);
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

}  // namespace esphome::mq_gas_sensors::testing
