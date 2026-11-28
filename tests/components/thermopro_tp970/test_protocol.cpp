#include "esphome/components/thermopro_tp970/thermopro_tp970_protocol.h"

#include <gtest/gtest.h>
#include <array>
#include <cmath>

namespace esphome::thermopro_tp970::testing {

TEST(ThermoProTP970Protocol, CapturedNotification) {
  const uint8_t data[]{0x10, 0x00, 0x36, 0x00, 0x2D, 0x0A, 0x36, 0x00};
  TemperatureReading reading;
  ASSERT_TRUE(decode_notification(data, sizeof(data), reading));
  EXPECT_FLOAT_EQ(reading.internal_temperature, 24.0f);
  EXPECT_FLOAT_EQ(reading.ambient_temperature, 24.0f);
  EXPECT_NEAR(reading.battery_level, 84.4696f, 0.001f);
}

TEST(ThermoProTP970Protocol, RejectsWrongLengthTypeAndProbeWithoutChangingOutput) {
  std::array<uint8_t, 9> data{0x10, 0x00, 0x36, 0x00, 0x2D, 0x0A, 0x36, 0x00, 0x00};
  TemperatureReading reading{1, 2, 3};
  for (size_t length = 0; length < data.size(); length++) {
    if (length != 8)
      EXPECT_FALSE(decode_notification(data.data(), length, reading));
  }
  EXPECT_FALSE(decode_notification(data.data(), data.size(), reading));
  data[0] = 0x20;
  EXPECT_FALSE(decode_notification(data.data(), 8, reading));
  data[0] = 0x10;
  data[1] = 1;
  EXPECT_FALSE(decode_notification(data.data(), 8, reading));
  EXPECT_FLOAT_EQ(reading.internal_temperature, 1);
  EXPECT_FLOAT_EQ(reading.ambient_temperature, 2);
  EXPECT_FLOAT_EQ(reading.battery_level, 3);
}

TEST(ThermoProTP970Protocol, LittleEndianAndNegativeTemperature) {
  const uint8_t data[]{0x10, 0x00, 0x00, 0x00, 0x2D, 0x0A, 0x2C, 0x01};
  TemperatureReading reading;
  ASSERT_TRUE(decode_notification(data, sizeof(data), reading));
  EXPECT_FLOAT_EQ(reading.internal_temperature, -30);
  EXPECT_FLOAT_EQ(reading.ambient_temperature, 270);
}

TEST(ThermoProTP970Protocol, InvalidFieldsAreIndependent) {
  for (uint8_t mask = 0; mask < 8; mask++) {
    std::array<uint8_t, 8> data{0x10, 0x00, 0x36, 0x00, 0x2D, 0x0A, 0x36, 0x00};
    for (uint8_t field = 0; field < 3; field++) {
      if ((mask & (1 << field)) != 0) {
        data[2 + 2 * field] = 0xFF;
        data[3 + 2 * field] = 0xFF;
      }
    }
    TemperatureReading reading;
    ASSERT_TRUE(decode_notification(data.data(), data.size(), reading));
    EXPECT_EQ(std::isnan(reading.internal_temperature), (mask & 1) != 0);
    EXPECT_EQ(std::isnan(reading.battery_level), (mask & 2) != 0);
    EXPECT_EQ(std::isnan(reading.ambient_temperature), (mask & 4) != 0);
    if ((mask & 1) == 0)
      EXPECT_FLOAT_EQ(reading.internal_temperature, 24);
    if ((mask & 4) == 0)
      EXPECT_FLOAT_EQ(reading.ambient_temperature, 24);
  }
}

TEST(ThermoProTP970Protocol, BatteryBoundsAndSentinel) {
  EXPECT_FLOAT_EQ(decode_battery_level(0), 0);
  EXPECT_FLOAT_EQ(decode_battery_level(65534), 100);
  EXPECT_TRUE(std::isnan(decode_battery_level(65535)));
  EXPECT_LT(decode_battery_level(2500), decode_battery_level(2600));
}

}  // namespace esphome::thermopro_tp970::testing
