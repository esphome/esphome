#include <gtest/gtest.h>

#include "esphome/components/modbus/modbus_helpers.h"

namespace esphome::modbus::helpers {

TEST(ModbusHelpersTest, PayloadToNumberRejectsOffsetAtEndOfBuffer) {
  const std::vector<uint8_t> data{0x12, 0x34};
  EXPECT_EQ(payload_to_number(data, SensorValueType::U_WORD, 2, 0xFFFFFFFF), 0);
}

TEST(ModbusHelpersTest, PayloadToNumberRejectsTruncatedMultiRegisterValue) {
  const std::vector<uint8_t> data{0x12, 0x34, 0x56};
  EXPECT_EQ(payload_to_number(data, SensorValueType::U_DWORD, 0, 0xFFFFFFFF), 0);
}

TEST(ModbusHelpersTest, PayloadToNumberDecodesValidWord) {
  const std::vector<uint8_t> data{0x12, 0x34};
  EXPECT_EQ(payload_to_number(data, SensorValueType::U_WORD, 0, 0xFFFFFFFF), 0x1234);
}

}  // namespace esphome::modbus::helpers
