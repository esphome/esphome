#include <gtest/gtest.h>
#include "../test_helpers.h"
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

TEST(CANopenSensor, PendingPollIsNotDuplicatedAndCompletionAllowsNextPoll) {
  testing::MockCANbus bus;
  testing::ProbeClient client(&bus);
  CANopenSensor sensor(&client, 32, 0x2000, 0, ValueType::VALUE_TYPE_UINT8);
  sensor.update();
  sensor.update();
  client.loop();
  sensor.update();
  client.on_frame_(0x5A0, false, false, {0x4F, 0, 0x20, 0, 42, 0, 0, 0});
  EXPECT_FLOAT_EQ(sensor.state, 42);
  client.loop();
  EXPECT_EQ(bus.sent.size(), 1);
  sensor.update();
  client.loop();
  EXPECT_EQ(bus.sent.size(), 2);
}

TEST(CANopenSensor, FullQueuePublishesNanAndCanPollAfterSpaceIsAvailable) {
  testing::MockCANbus bus;
  testing::ProbeClient client(&bus);
  CANopenSensor sensor(&client, 32, 0x2000, 0, ValueType::VALUE_TYPE_UINT8);
  for (unsigned i = 0; i < 16; i++)
    ASSERT_TRUE(client.write_sdo(33, 0x2000, 0, i, 1));
  sensor.update();
  EXPECT_TRUE(std::isnan(sensor.state));
  EXPECT_TRUE(sensor.status_has_warning());
  for (unsigned i = 0; i < 16; i++) {
    client.loop();
    client.on_frame_(0x5A1, false, false, {0x60, 0, 0x20, 0, 0, 0, 0, 0});
  }
  sensor.update();
  client.loop();
  ASSERT_EQ(bus.sent.size(), 17);
  client.on_frame_(0x5A0, false, false, {0x4F, 0, 0x20, 0, 42, 0, 0, 0});
  EXPECT_FLOAT_EQ(sensor.state, 42);
  EXPECT_FALSE(sensor.status_has_warning());
}

TEST(CANopenSensor, TimeoutAllowsPollingAgainAndOtherNodeCannotClearWarning) {
  testing::MockCANbus bus;
  testing::ProbeClient client(&bus);
  CANopenSensor offline(&client, 33, 0x2000, 0, ValueType::VALUE_TYPE_UINT8);
  CANopenSensor healthy(&client, 32, 0x2000, 0, ValueType::VALUE_TYPE_UINT8);
  offline.update();
  healthy.update();
  client.loop();
  client.expire();
  client.loop();
  EXPECT_TRUE(std::isnan(offline.state));
  EXPECT_TRUE(offline.status_has_warning());
  client.loop();
  client.on_frame_(0x5A0, false, false, {0x4F, 0, 0x20, 0, 42, 0, 0, 0});
  EXPECT_TRUE(offline.status_has_warning());
  EXPECT_FALSE(healthy.status_has_warning());
  EXPECT_FALSE(client.status_has_warning());
  offline.update();
  client.loop();
  ASSERT_EQ(bus.sent.size(), 3);
  client.on_frame_(0x5A1, false, false, {0x4F, 0, 0x20, 0, 7, 0, 0, 0});
  EXPECT_FLOAT_EQ(offline.state, 7);
  EXPECT_FALSE(offline.status_has_warning());
}

}  // namespace esphome::canopen::sensor_testing
