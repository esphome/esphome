#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/modbus_monitor/sensor/modbus_monitor_sensor.h"

namespace esphome::modbus_monitor::testing {

using modbus::helpers::SensorValueType;

namespace {

float decode(SensorValueType value_type, std::vector<uint8_t> bytes) {
  ModbusMonitorSensor sensor{static_cast<uint8_t>(0x01), 100, value_type};
  EXPECT_EQ(sensor.register_count * 2u, bytes.size());
  sensor.parse_and_publish(bytes);
  return sensor.state;
}

}  // namespace

// The sensor decodes with modbus_controller's helper, so each value type means what it means there.
TEST(ModbusMonitorSensor, DecodesEachValueTypeAsTheControllerDoes) {
  EXPECT_FLOAT_EQ(decode(SensorValueType::U_WORD, {0xF7, 0xB4}), 63412.0f);
  EXPECT_FLOAT_EQ(decode(SensorValueType::S_WORD, {0xF7, 0xB4}), -2124.0f);
  EXPECT_FLOAT_EQ(decode(SensorValueType::U_DWORD, {0x00, 0x01, 0x00, 0x02}), 65538.0f);
  EXPECT_FLOAT_EQ(decode(SensorValueType::U_DWORD_R, {0x00, 0x02, 0x00, 0x01}), 65538.0f);
  EXPECT_FLOAT_EQ(decode(SensorValueType::S_DWORD, {0xFF, 0xFF, 0xFF, 0xFE}), -2.0f);
  EXPECT_FLOAT_EQ(decode(SensorValueType::FP32, {0x41, 0x48, 0x00, 0x00}), 12.5f);    // 0x41480000
  EXPECT_FLOAT_EQ(decode(SensorValueType::FP32_R, {0x00, 0x00, 0x41, 0x48}), 12.5f);  // low word first
  EXPECT_FLOAT_EQ(decode(SensorValueType::U_QWORD, {0, 0, 0, 0, 0, 1, 0, 0}), 65536.0f);
}

// The register count comes from the value type, so the decoder asks for every register a value spans.
TEST(ModbusMonitorSensor, RegisterCountFollowsTheValueType) {
  EXPECT_EQ((ModbusMonitorSensor{static_cast<uint8_t>(1), 0, SensorValueType::S_WORD}.register_count), 1);
  EXPECT_EQ((ModbusMonitorSensor{static_cast<uint8_t>(1), 0, SensorValueType::FP32}.register_count), 2);
  EXPECT_EQ((ModbusMonitorSensor{static_cast<uint8_t>(1), 0, SensorValueType::S_QWORD_R}.register_count), 4);
}

}  // namespace esphome::modbus_monitor::testing
