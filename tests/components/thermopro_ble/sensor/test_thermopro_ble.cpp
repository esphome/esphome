#include "../common.h"
#include "esphome/components/thermopro_ble/thermopro_ble.h"

#include <gtest/gtest.h>

#include <cmath>

namespace esphome::thermopro_ble::testing {

class ThermoProBLETest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_address(device_from(MAC, nullptr, {}).address_uint64());
    this->hub_.set_temperature_sensor(&this->temperature_);
    this->hub_.set_external_temperature_sensor(&this->external_temperature_);
    this->hub_.set_humidity_sensor(&this->humidity_);
    this->hub_.set_battery_level_sensor(&this->battery_level_);
    this->hub_.set_signal_strength_sensor(&this->signal_strength_);
  }

  bool parse(const char *name, const std::vector<uint8_t> &record, const uint8_t *mac = MAC) {
    return this->hub_.parse_device(device_from(mac, name, record));
  }

  bool nothing_published() const {
    return !this->temperature_.has_state() && !this->external_temperature_.has_state() &&
           !this->humidity_.has_state() && !this->battery_level_.has_state();
  }

  ThermoProBLE hub_;
  sensor::Sensor temperature_;
  sensor::Sensor external_temperature_;
  sensor::Sensor humidity_;
  sensor::Sensor battery_level_;
  sensor::Sensor signal_strength_;
};

TEST_F(ThermoProBLETest, TP357) {
  EXPECT_TRUE(this->parse("TP357 (2142)", TP357));
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.1f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 29.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
  EXPECT_FLOAT_EQ(this->signal_strength_.state, RSSI);
  EXPECT_FALSE(this->external_temperature_.has_state());
}

TEST_F(ThermoProBLETest, TP357SSevenByteFrame) {
  EXPECT_TRUE(this->parse("TP357S (2142)", TP357S));
  EXPECT_FLOAT_EQ(this->temperature_.state, 23.8f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 41.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
}

TEST_F(ThermoProBLETest, TP393) {
  EXPECT_TRUE(this->parse("TP393 (9376)", TP393));
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.2f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 53.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
}

TEST_F(ThermoProBLETest, TP3ShortFrameRejected) {
  EXPECT_FALSE(this->parse("TP357 (2142)", {0xC2, 0xF1, 0x00, 0x1D, 0x02}));
  EXPECT_TRUE(this->nothing_published());
}

TEST_F(ThermoProBLETest, TP960R) {
  EXPECT_TRUE(this->parse("TP960R", TP960R));
  EXPECT_FLOAT_EQ(this->temperature_.state, 28.0f);
  EXPECT_FLOAT_EQ(this->external_temperature_.state, 26.0f);
  EXPECT_NEAR(this->battery_level_.state, 9.10f, 0.01f);
  EXPECT_FALSE(this->humidity_.has_state());
}

TEST_F(ThermoProBLETest, TP970RBatteryClampedTo100) {
  EXPECT_TRUE(this->parse("TP970R", TP970R));
  EXPECT_FLOAT_EQ(this->temperature_.state, 23.0f);
  EXPECT_FLOAT_EQ(this->external_temperature_.state, 23.0f);
  EXPECT_FLOAT_EQ(this->battery_level_.state, 100.0f);
}

TEST_F(ThermoProBLETest, TP96WrongSizeRejected) {
  EXPECT_FALSE(this->parse("TP960R", {0x00, 0x3A, 0x00, 0x30, 0x08, 0x38}));
  EXPECT_TRUE(this->nothing_published());
}

TEST_F(ThermoProBLETest, TP972Battery) {
  EXPECT_TRUE(this->parse("TP972S", TP972S));
  EXPECT_NEAR(this->battery_level_.state, 90.43f, 0.01f);
}

TEST_F(ThermoProBLETest, OversizedFrameRejected) {
  EXPECT_FALSE(this->parse("TP972S", INVALID_TP972));
  EXPECT_TRUE(this->nothing_published());
}

TEST_F(ThermoProBLETest, OtherMacIgnored) {
  EXPECT_FALSE(this->parse("TP357 (2142)", TP357, OTHER_MAC));
  EXPECT_TRUE(this->nothing_published());
  EXPECT_FALSE(this->signal_strength_.has_state());
}

TEST_F(ThermoProBLETest, UnknownNameIgnored) {
  EXPECT_FALSE(this->parse("TP25", TP357));
  EXPECT_FALSE(this->parse(nullptr, TP357));
  EXPECT_TRUE(this->nothing_published());
  EXPECT_FALSE(this->signal_strength_.has_state());
}

}  // namespace esphome::thermopro_ble::testing
