#include <gtest/gtest.h>
#include "esphome/components/canopen/sensor/canopen_sensor.h"

namespace esphome::canopen::sensor_testing {

class ProbeSensor : public CANopenSensor {
 public:
  explicit ProbeSensor(ValueType type) : CANopenSensor(nullptr, 32, 0x2000, 0, type) {}
  using CANopenSensor::on_result_;
};

TEST(CANopenSensor, SignedValuesAreExtendedAtConfiguredWidth) {
  for (const auto type : {ValueType::VALUE_TYPE_INT8, ValueType::VALUE_TYPE_INT16, ValueType::VALUE_TYPE_INT32}) {
    ProbeSensor sensor(type);
    const uint8_t size = type == ValueType::VALUE_TYPE_INT8 ? 1 : type == ValueType::VALUE_TYPE_INT16 ? 2 : 4;
    sensor.on_result_(SDOResult::SDO_RESULT_SUCCESS, UINT32_MAX, size);
    EXPECT_FLOAT_EQ(sensor.state, -1.0f);
  }
}

TEST(CANopenSensor, Float32PreservesWireBitPattern) {
  ProbeSensor sensor(ValueType::VALUE_TYPE_FLOAT32);
  sensor.on_result_(SDOResult::SDO_RESULT_SUCCESS, 0x3FC00000, 4);
  EXPECT_FLOAT_EQ(sensor.state, 1.5f);
}

TEST(CANopenSensor, FailureOrWrongWidthPublishesNanAndNextSuccessRecovers) {
  ProbeSensor sensor(ValueType::VALUE_TYPE_UINT16);
  sensor.on_result_(SDOResult::SDO_RESULT_SUCCESS, 42, 1);
  EXPECT_TRUE(std::isnan(sensor.state));
  sensor.on_result_(SDOResult::SDO_RESULT_TIMEOUT, 0, 0);
  EXPECT_TRUE(std::isnan(sensor.state));
  sensor.on_result_(SDOResult::SDO_RESULT_SUCCESS, 65535, 2);
  EXPECT_FLOAT_EQ(sensor.state, 65535.0f);
}

}  // namespace esphome::canopen::sensor_testing
