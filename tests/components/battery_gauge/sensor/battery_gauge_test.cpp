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

// --- persistence scale-factor conversions used by setup() and publish_() ---

TEST_F(BatteryGaugeTest, FractionToPercentageX10ConvertsFractionToTenthsOfPercent) {
  // 0.5 (50%) -> 500 tenths-of-a-percent
  EXPECT_EQ(TestableBatteryGauge::fraction_to_percentage_x10(0.5f), 500u);
  EXPECT_EQ(TestableBatteryGauge::fraction_to_percentage_x10(0.0f), 0u);
  EXPECT_EQ(TestableBatteryGauge::fraction_to_percentage_x10(1.0f), 1000u);
}

TEST_F(BatteryGaugeTest, FractionToPercentageX10Rounds) {
  // 0.3333... -> 333.33... tenths-of-a-percent, rounds to 333, not truncates.
  EXPECT_EQ(TestableBatteryGauge::fraction_to_percentage_x10(1.0f / 3.0f), 333u);
  // 0.6666... -> 666.66... tenths-of-a-percent, rounds up to 667.
  EXPECT_EQ(TestableBatteryGauge::fraction_to_percentage_x10(2.0f / 3.0f), 667u);
}

TEST_F(BatteryGaugeTest, PercentageX10ToStateConvertsBackToAh) {
  // 500 tenths-of-a-percent (50%) of a 10 Ah battery -> 5 Ah.
  EXPECT_FLOAT_EQ(TestableBatteryGauge::percentage_x10_to_state(500u, 10.0f), 5.0f);
  EXPECT_FLOAT_EQ(TestableBatteryGauge::percentage_x10_to_state(0u, 10.0f), 0.0f);
  EXPECT_FLOAT_EQ(TestableBatteryGauge::percentage_x10_to_state(1000u, 10.0f), 10.0f);
}

TEST_F(BatteryGaugeTest, PersistenceScalingRoundTripsThroughFractionAndState) {
  // A round trip through the same encoding setup() uses to restore persisted charge:
  // fraction -> percentage_x10 (as stored on first boot) -> state in Ah (as restored on later boots).
  constexpr float capacity = 10.0f;
  constexpr float initial_state = 0.5f;

  auto percentage_x10 = TestableBatteryGauge::fraction_to_percentage_x10(initial_state);
  auto restored_state = TestableBatteryGauge::percentage_x10_to_state(percentage_x10, capacity);

  EXPECT_FLOAT_EQ(restored_state, initial_state * capacity);
}

}  // namespace esphome::battery_gauge::testing
