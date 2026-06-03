#include "../common.h"
#include "esphome/core/hal.h"
#include <cmath>

namespace esphome::battery_gauge::testing {

// One hour expressed in milliseconds, used to drive deterministic-direction
// charge integration tests by back-dating last_time_.
static constexpr uint32_t ONE_HOUR_MS = 3600u * 1000u;

// --- publish_(): clamping, percentage and persistence ---

TEST_F(BatteryGaugeTest, PublishClampsBelowZero) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.publish_(-5.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 0.0f);
  EXPECT_FLOAT_EQ(gauge.state, 0.0f);
}

TEST_F(BatteryGaugeTest, PublishClampsAboveCapacity) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.publish_(20.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 10.0f);
  EXPECT_FLOAT_EQ(gauge.state, 100.0f);
}

TEST_F(BatteryGaugeTest, PublishComputesPercentage) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.publish_(2.5f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 2.5f);
  EXPECT_FLOAT_EQ(gauge.state, 25.0f);
}

TEST_F(BatteryGaugeTest, PublishStoresRoundedPercentageTimesTen) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  // 0.333 Ah of 10 Ah -> 3.33% -> round(33.3) -> 33
  gauge.publish_(0.333f);
  EXPECT_EQ(gauge.charge_percentage_, 33u);
}

// --- on_current_(): integration of current over time ---

TEST_F(BatteryGaugeTest, OnCurrentIgnoresNonFinite) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.last_current_ = 1.0f;
  gauge.charge_state_ = 5.0f;

  gauge.on_current_(NAN);
  gauge.on_current_(INFINITY);

  // Invalid samples are dropped without touching any state.
  EXPECT_FLOAT_EQ(gauge.last_current_, 1.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

TEST_F(BatteryGaugeTest, OnCurrentFirstSampleEstablishesBaseline) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.charge_state_ = 5.0f;
  // last_time_ defaults to 0 -> first sample only records state, no integration.
  gauge.on_current_(2.0f);

  EXPECT_FLOAT_EQ(gauge.last_current_, 2.0f);
  EXPECT_FLOAT_EQ(gauge.filtered_current_, 2.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

TEST_F(BatteryGaugeTest, OnCurrentPositiveIncreasesCharge) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = 2.0f;
  gauge.filtered_current_ = 2.0f;
  // Pretend the previous sample was ~1 hour ago so a positive current adds charge.
  gauge.last_time_ = millis() - ONE_HOUR_MS;

  gauge.on_current_(2.0f);

  EXPECT_GT(gauge.charge_state_, 5.0f);
  EXPECT_LT(gauge.charge_state_, 10.0f);
}

TEST_F(BatteryGaugeTest, OnCurrentNegativeDecreasesCharge) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = -2.0f;
  gauge.filtered_current_ = -2.0f;
  gauge.last_time_ = millis() - ONE_HOUR_MS;

  gauge.on_current_(-2.0f);

  EXPECT_LT(gauge.charge_state_, 5.0f);
  EXPECT_GE(gauge.charge_state_, 0.0f);
}

// --- on_voltage_(): full-charge resynchronisation ---

TEST_F(BatteryGaugeTest, OnVoltageFirstSampleSeedsFilter) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  // filtered_voltage_ starts as NAN; the first sample seeds it and returns.
  gauge.on_voltage_(3.7f);
  EXPECT_FLOAT_EQ(gauge.filtered_voltage_, 3.7f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 0.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageIgnoredUnderLoad) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.filtered_voltage_ = 3.7f;
  // |current| above capacity/20 (0.5 A) -> voltage is not used for adjustment.
  gauge.last_current_ = 5.0f;

  gauge.on_voltage_(4.2f);

  // Filter left untouched, no resync.
  EXPECT_FLOAT_EQ(gauge.filtered_voltage_, 3.7f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 0.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageFullChargeResyncsToCapacity) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = 0.0f;
  gauge.filtered_voltage_ = 4.2f;
  gauge.filtered_current_ = 0.0f;  // below capacity * 0.02 (0.2 A)

  gauge.on_voltage_(4.2f);

  EXPECT_FLOAT_EQ(gauge.charge_state_, 10.0f);
  EXPECT_FLOAT_EQ(gauge.state, 100.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageNoResyncWhileStillCharging) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = 0.0f;
  gauge.filtered_voltage_ = 4.2f;
  // A significant charge current (>= capacity * 0.02) means "not full yet".
  gauge.filtered_current_ = 1.0f;

  gauge.on_voltage_(4.2f);

  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageNoResyncBelowMaxVoltage) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = 0.0f;
  gauge.filtered_voltage_ = 4.0f;
  gauge.filtered_current_ = 0.0f;

  gauge.on_voltage_(4.0f);

  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

// --- configuration setters ---

TEST_F(BatteryGaugeTest, SetInitialState) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f);
  gauge.set_initial_state(0.5f);
  EXPECT_FLOAT_EQ(gauge.initial_state_, 0.5f);
}

}  // namespace esphome::battery_gauge::testing
