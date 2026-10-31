#include "../common.h"
#include "esphome/components/thermopro_ble/thermopro_ble.h"

#include <gtest/gtest.h>

#include <cmath>

namespace esphome::thermopro_ble::testing {

class ThermoProBLETest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_temperature_sensor(&this->temperature_);
    this->hub_.set_external_temperature_sensor(&this->external_temperature_);
    this->hub_.set_humidity_sensor(&this->humidity_);
    this->hub_.set_battery_level_sensor(&this->battery_level_);
    this->hub_.set_signal_strength_sensor(&this->signal_strength_);
  }

  bool parse_(const char *name, const std::vector<uint8_t> &record, const uint8_t *mac = MAC) {
    return this->hub_.parse_device(device_from(mac, name, record));
  }

  bool nothing_published_() const {
    return !this->temperature_.has_state() && !this->external_temperature_.has_state() &&
           !this->humidity_.has_state() && !this->battery_level_.has_state();
  }

  ThermoProBLE hub_{device_from(MAC, nullptr, {}).address_uint64()};
  sensor::Sensor temperature_;
  sensor::Sensor external_temperature_;
  sensor::Sensor humidity_;
  sensor::Sensor battery_level_;
  sensor::Sensor signal_strength_;
};

TEST_F(ThermoProBLETest, TP357) {
  EXPECT_TRUE(this->parse_("TP357 (2142)", TP357));
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.1f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 29.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
  EXPECT_FLOAT_EQ(this->signal_strength_.state, RSSI);
  EXPECT_FALSE(this->external_temperature_.has_state());
}

TEST_F(ThermoProBLETest, TP357SSevenByteFrame) {
  EXPECT_TRUE(this->parse_("TP357S (2142)", TP357S));
  EXPECT_FLOAT_EQ(this->temperature_.state, 23.8f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 41.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
}

TEST_F(ThermoProBLETest, TP393) {
  EXPECT_TRUE(this->parse_("TP393 (9376)", TP393));
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.2f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 53.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
}

TEST_F(ThermoProBLETest, TP3ShortFrameRejected) {
  EXPECT_FALSE(this->parse_("TP357 (2142)", {0xC2, 0xF1, 0x00, 0x1D, 0x02}));
  EXPECT_TRUE(this->nothing_published_());
}

TEST_F(ThermoProBLETest, TP960R) {
  EXPECT_TRUE(this->parse_("TP960R", TP960R));
  EXPECT_FLOAT_EQ(this->temperature_.state, 28.0f);
  EXPECT_FLOAT_EQ(this->external_temperature_.state, 26.0f);
  EXPECT_NEAR(this->battery_level_.state, 9.10f, 0.01f);
  EXPECT_FALSE(this->humidity_.has_state());
}

TEST_F(ThermoProBLETest, TP970RBatteryClampedTo100) {
  EXPECT_TRUE(this->parse_("TP970R", TP970R));
  EXPECT_FLOAT_EQ(this->temperature_.state, 23.0f);
  EXPECT_FLOAT_EQ(this->external_temperature_.state, 23.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
}

TEST_F(ThermoProBLETest, TP96WrongSizeRejected) {
  EXPECT_FALSE(this->parse_("TP960R", {0x00, 0x3A, 0x00, 0x30, 0x08, 0x38}));
  EXPECT_TRUE(this->nothing_published_());
}

TEST_F(ThermoProBLETest, TP3BatteryLevels) {
  std::vector<uint8_t> record = TP357;
  record[4] = 0x00;
  EXPECT_TRUE(this->parse_("TP357 (2142)", record));
  EXPECT_FLOAT_EQ(this->battery_level_.state, 1.0f);
  record[4] = 0x01;
  EXPECT_TRUE(this->parse_("TP357 (2142)", record));
  EXPECT_FLOAT_EQ(this->battery_level_.state, 50.0f);
}

TEST_F(ThermoProBLETest, TP3UnknownBatteryNotPublished) {
  std::vector<uint8_t> record = TP357;
  record[4] = 0x03;
  EXPECT_TRUE(this->parse_("TP357 (2142)", record));
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.1f);
  EXPECT_FALSE(this->battery_level_.has_state());
}

TEST_F(ThermoProBLETest, TP357SInvalidReadingNotPublished) {
  std::vector<uint8_t> record = TP357S;
  record[1] = record[2] = record[3] = 0xFF;
  EXPECT_TRUE(this->parse_("TP357S (3C62)", record));
  EXPECT_FALSE(this->temperature_.has_state());
  EXPECT_FALSE(this->humidity_.has_state());
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
}

TEST_F(ThermoProBLETest, TP3NegativeTemperature) {
  std::vector<uint8_t> record = TP357;
  record[1] = 0xCE;
  record[2] = 0xFF;
  EXPECT_TRUE(this->parse_("TP357 (2142)", record));
  EXPECT_FLOAT_EQ(this->temperature_.state, -5.0f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 29.0f);
}

TEST_F(ThermoProBLETest, TP96ThirteenByteFrame) {
  EXPECT_TRUE(this->parse_("TP960R", with_station_mac(TP960R)));
  EXPECT_FLOAT_EQ(this->temperature_.state, 28.0f);
  EXPECT_FLOAT_EQ(this->external_temperature_.state, 26.0f);
  EXPECT_NEAR(this->battery_level_.state, 9.10f, 0.01f);
}

TEST_F(ThermoProBLETest, TP972ThirteenByteFrame) {
  EXPECT_TRUE(this->parse_("TP972S", with_station_mac(TP972S_7B)));
  EXPECT_FLOAT_EQ(this->temperature_.state, 21.0f);
  EXPECT_FLOAT_EQ(this->external_temperature_.state, 21.0f);
  EXPECT_NEAR(this->battery_level_.state, 87.34f, 0.01f);
}

// Upstream thermopro-ble rounds ambient down to 15 and 30 °C; the sensor keeps the fraction.
TEST_F(ThermoProBLETest, TP972S) {
  EXPECT_TRUE(this->parse_("TP972S", TP972S));
  EXPECT_NEAR(this->temperature_.state, 3.56f, 0.01f);
  EXPECT_NEAR(this->external_temperature_.state, 15.56f, 0.01f);
  EXPECT_NEAR(this->battery_level_.state, 90.43f, 0.01f);
}

TEST_F(ThermoProBLETest, TP972S2) {
  EXPECT_TRUE(this->parse_("TP972S", TP972S_2));
  EXPECT_NEAR(this->temperature_.state, 23.31f, 0.01f);
  EXPECT_NEAR(this->external_temperature_.state, 30.56f, 0.01f);
  EXPECT_NEAR(this->battery_level_.state, 94.65f, 0.01f);
}

TEST_F(ThermoProBLETest, TP970MultiPointFrame) {
  EXPECT_TRUE(this->parse_("TP970R", TP972S));
  EXPECT_NEAR(this->temperature_.state, 3.56f, 0.01f);
  EXPECT_NEAR(this->external_temperature_.state, 15.56f, 0.01f);
}

TEST_F(ThermoProBLETest, LargestFrameAccepted) {
  std::vector<uint8_t> record = TP357;
  record.resize(24);
  EXPECT_TRUE(this->parse_("TP357 (2142)", record));
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.1f);
}

TEST_F(ThermoProBLETest, OversizedFrameRejected) {
  EXPECT_FALSE(this->parse_("TP972S", INVALID_TP972));
  EXPECT_TRUE(this->nothing_published_());
}

TEST_F(ThermoProBLETest, OtherMacIgnored) {
  EXPECT_FALSE(this->parse_("TP357 (2142)", TP357, OTHER_MAC));
  EXPECT_TRUE(this->nothing_published_());
  EXPECT_FALSE(this->signal_strength_.has_state());
}

TEST_F(ThermoProBLETest, UnknownNameIgnored) {
  EXPECT_FALSE(this->parse_("TP25", TP357));
  EXPECT_FALSE(this->parse_(nullptr, TP357));
  EXPECT_TRUE(this->nothing_published_());
  EXPECT_FALSE(this->signal_strength_.has_state());
}

}  // namespace esphome::thermopro_ble::testing
