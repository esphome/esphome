#include "esphome/components/thermopro_ble/thermopro_ble_protocol.h"

#include <gtest/gtest.h>
#include <array>
#include <cmath>

namespace esphome::thermopro_ble::testing {

TEST(ThermoProBLEProtocol, KnownNotification) {
  const uint8_t data[]{0x10, 0x00, 0x36, 0x00, 0x2D, 0x0A, 0x36, 0x00};
  ParseResult reading;
  ASSERT_TRUE(parse_tp970_notification(data, sizeof(data), reading));
  EXPECT_FLOAT_EQ(reading.temperature, 24.0f);
  EXPECT_FLOAT_EQ(reading.external_temperature, 24.0f);
  EXPECT_NEAR(reading.battery_level, 84.4696f, 0.001f);
}

TEST(ThermoProBLEProtocol, RejectsWrongLengthTypeAndProbeWithoutChangingOutput) {
  std::array<uint8_t, 9> data{0x10, 0x00, 0x36, 0x00, 0x2D, 0x0A, 0x36, 0x00, 0x00};
  ParseResult reading{1, 2, NAN, 3};
  for (size_t length = 0; length < data.size(); length++) {
    if (length != 8)
      EXPECT_FALSE(parse_tp970_notification(data.data(), length, reading));
  }
  EXPECT_FALSE(parse_tp970_notification(data.data(), data.size(), reading));
  data[0] = 0x20;
  EXPECT_FALSE(parse_tp970_notification(data.data(), 8, reading));
  data[0] = 0x10;
  data[1] = 1;
  EXPECT_FALSE(parse_tp970_notification(data.data(), 8, reading));
  EXPECT_FLOAT_EQ(reading.temperature, 1);
  EXPECT_FLOAT_EQ(reading.external_temperature, 2);
  EXPECT_FLOAT_EQ(reading.battery_level, 3);
}

TEST(ThermoProBLEProtocol, LittleEndianAndNegativeTemperature) {
  const uint8_t data[]{0x10, 0x00, 0x00, 0x00, 0x2D, 0x0A, 0x2C, 0x01};
  ParseResult reading;
  ASSERT_TRUE(parse_tp970_notification(data, sizeof(data), reading));
  EXPECT_FLOAT_EQ(reading.temperature, -30);
  EXPECT_FLOAT_EQ(reading.external_temperature, 270);
}

TEST(ThermoProBLEProtocol, InvalidFieldsAreIndependent) {
  for (uint8_t mask = 0; mask < 8; mask++) {
    std::array<uint8_t, 8> data{0x10, 0x00, 0x36, 0x00, 0x2D, 0x0A, 0x36, 0x00};
    for (uint8_t field = 0; field < 3; field++) {
      if ((mask & (1 << field)) != 0) {
        data[2 + 2 * field] = 0xFF;
        data[3 + 2 * field] = 0xFF;
      }
    }
    ParseResult reading;
    ASSERT_TRUE(parse_tp970_notification(data.data(), data.size(), reading));
    EXPECT_EQ(std::isnan(reading.temperature), (mask & 1) != 0);
    EXPECT_EQ(std::isnan(reading.battery_level), (mask & 2) != 0);
    EXPECT_EQ(std::isnan(reading.external_temperature), (mask & 4) != 0);
    if ((mask & 1) == 0)
      EXPECT_FLOAT_EQ(reading.temperature, 24);
    if ((mask & 4) == 0)
      EXPECT_FLOAT_EQ(reading.external_temperature, 24);
  }
}

TEST(ThermoProBLEProtocol, BatteryBoundsAndSentinel) {
  EXPECT_FLOAT_EQ(tp96_battery(0), 0);
  EXPECT_FLOAT_EQ(tp96_battery(65534), 100);
  EXPECT_TRUE(std::isnan(tp96_battery(65535)));
  EXPECT_LT(tp96_battery(2500), tp96_battery(2600));
}

}  // namespace esphome::thermopro_ble::testing

namespace esphome::thermopro_ble::testing {
TEST(ThermoProBLEProtocol, AdvertisementAndGattShareDecoder) {
  const uint8_t data[]{0x10, 0, 0x36, 0, 0x2d, 0x0a, 0x36, 0};
  ParseResult adv, gatt;
  ASSERT_TRUE(parse_tp96(data + 1, 7, adv));
  ASSERT_TRUE(parse_tp970_notification(data, 8, gatt));
  EXPECT_FLOAT_EQ(adv.temperature, gatt.temperature);
  EXPECT_FLOAT_EQ(adv.external_temperature, gatt.external_temperature);
  EXPECT_FLOAT_EQ(adv.battery_level, gatt.battery_level);
}
TEST(ThermoProBLEProtocol, NotificationFreshnessAndRecovery) {
  NotificationState state;
  EXPECT_FALSE(state.active);
  state.record(100);
  EXPECT_TRUE(state.active);
  EXPECT_EQ(state.count, 1);
  EXPECT_FALSE(state.expired(15099, 15000));
  EXPECT_TRUE(state.expired(15100, 15000));
  state.clear();
  EXPECT_FALSE(state.active);
  EXPECT_FALSE(state.expired(16000, 15000));
  state.record(17000);
  EXPECT_EQ(state.count, 2);
  EXPECT_FALSE(state.expired(17001, 15000));
  state.record(0xfffffff0U);
  EXPECT_TRUE(state.expired(0x20U, 48));
}
}  // namespace esphome::thermopro_ble::testing
