#include <gtest/gtest.h>

#include "esphome/components/ms8607/ms8607.h"

namespace esphome::ms8607::testing {

class TestableMS8607Component : public MS8607Component {
 public:
  using MS8607Component::MS8607Component;

  using CalibrationValues = MS8607Component::CalibrationValues;
  using CompensatedTemperature = MS8607Component::CompensatedTemperature;

  using MS8607Component::compensated_temperature;
  using MS8607Component::compensated_pressure;
  using MS8607Component::compensated_humidity;
};

static TestableMS8607Component::CalibrationValues createCalibrationValues() {
  TestableMS8607Component::CalibrationValues values;

  return values;
}

TEST(TemperatureCompensationTest, Foo) {
  TestableMS8607Component::CalibrationValues calibration_values = createCalibrationValues();

  EXPECT_EQ(calibration_values.pressure_sensitivity, 0);
}

}  // namespace esphome::ms8607::testing
