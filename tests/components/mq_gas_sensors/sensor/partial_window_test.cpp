#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/mq_gas_sensors/mq_gas_sensor.h"

namespace esphome::mq_gas_sensors::testing {

/// Voltage source that fails every other conversion while `flaky` is set.
class FlakySampler : public voltage_sampler::VoltageSampler {
 public:
  float sample() override {
    this->fail_next = !this->fail_next;
    return this->flaky && this->fail_next ? NAN : 1.0f;
  }
  bool flaky{true};
  bool fail_next{false};
};

TEST(PartialWindow, IsReportedOnceAndAveragesTheValidConversions) {
  FlakySampler sampler;
  MQGasSensor sensor{&sampler};
  sensor.set_a(976.97f);
  sensor.set_b(-0.688f);
  sensor.set_regression_method(RegressionMethod::REGRESSION_METHOD_EXPONENTIAL);
  sensor.set_r0(10.0f);
  sensor.set_samples(2);
  sensor::Sensor voltage;
  sensor.set_voltage_sensor(&voltage);
  text_sensor::TextSensor log;
  std::vector<std::string> reports;
  log.add_on_state_callback([&reports](const std::string &message) {
    if (message.find("only 1 of 2 conversions") != std::string::npos)
      reports.push_back(message);
  });
  sensor.set_log_sensor(&log);

  sensor.update();
  sensor.update();

  // The failed conversion is left out of the average instead of poisoning it.
  EXPECT_FLOAT_EQ(1.0f, voltage.state);
  EXPECT_TRUE(std::isfinite(sensor.state));
  EXPECT_EQ(1u, reports.size());

  // A full window ends the episode, so the next partial one is reported again.
  sampler.flaky = false;
  sensor.update();
  sampler.flaky = true;
  sensor.update();
  EXPECT_EQ(2u, reports.size());
}

}  // namespace esphome::mq_gas_sensors::testing
