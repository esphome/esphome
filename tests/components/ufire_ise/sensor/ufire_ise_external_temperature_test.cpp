#include <gtest/gtest.h>

#include <cmath>

#include "../common.h"
#include "esphome/components/ufire_ise/ufire_ise.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::ufire_ise::testing {

class ExternalTemperatureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ise_.set_i2c_bus(&this->bus_);
    this->ise_.set_i2c_address(0x3F);
    this->ise_.set_ph_sensor(&this->ph_);
    this->ise_.set_external_temperature_sensor(&this->external_);
    // -59.2 mV reads as pH 8
    this->bus_.mv = -59.2f;
  }

  // Runs update() and the pH read it schedules 750 ms later
  float measure_() {
    this->ise_.update();
    delay(760);  // NOLINT
    App.scheduler.call(millis());
    return this->ph_.state;
  }

  MillivoltBus bus_;
  sensor::Sensor ph_;
  sensor::Sensor external_;
  UFireISEComponent ise_;
};

TEST_F(ExternalTemperatureTest, NoReadingYetPublishesUncorrectedPh) { EXPECT_NEAR(this->measure_(), 8.0f, 0.001f); }

TEST_F(ExternalTemperatureTest, NanReadingPublishesUncorrectedPh) {
  this->external_.publish_state(NAN);
  EXPECT_NEAR(this->measure_(), 8.0f, 0.001f);
}

TEST_F(ExternalTemperatureTest, ReadingCorrectsPh) {
  // 45 °C is two steps from 25 °C, one pH from 7, and above 35 °C at pH 8: 8 - 2 * 0.03
  this->external_.publish_state(45.0f);
  EXPECT_NEAR(this->measure_(), 7.94f, 0.001f);
}

}  // namespace esphome::ufire_ise::testing
