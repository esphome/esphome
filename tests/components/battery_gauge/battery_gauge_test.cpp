#include "common.h"
#include "esphome/core/hal.h"
#include <cmath>

namespace esphome::battery_gauge::testing {

// One hour expressed in milliseconds, used to drive deterministic-direction
// charge integration tests by back-dating last_time_.
static constexpr uint32_t ONE_HOUR_MS = 3600u * 1000u;

// --- publish_(): clamping, percentage and persistence ---

TEST_F(BatteryGaugeTest, PublishClampsBelowZero) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.publish_(-5.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 0.0f);
  EXPECT_FLOAT_EQ(gauge.soc_sensor_.state, 0.0f);
}

TEST_F(BatteryGaugeTest, PublishClampsAboveCapacity) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.publish_(20.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 10.0f);
  EXPECT_FLOAT_EQ(gauge.soc_sensor_.state, 100.0f);
}

TEST_F(BatteryGaugeTest, PublishComputesPercentage) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.publish_(2.5f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 2.5f);
  EXPECT_FLOAT_EQ(gauge.soc_sensor_.state, 25.0f);
}

TEST_F(BatteryGaugeTest, PublishStoresRoundedPercentageTimesTen) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  // 0.333 Ah of 10 Ah -> 3.33% -> round(33.3) -> 33
  gauge.publish_(0.333f);
  EXPECT_EQ(gauge.persisted_.percentage_x10, 33u);
}

// --- publish_(): preference-save hysteresis ---

TEST_F(BatteryGaugeTest, PublishSkipsSaveForSmallChange) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.publish_(5.0f);  // 50.0% -> persisted as 500
  ASSERT_EQ(gauge.persisted_.percentage_x10, 500u);

  gauge.publish_(5.02f);  // 50.2% -> only a 2-tenths-of-a-percent move, below the save threshold
  EXPECT_EQ(gauge.persisted_.percentage_x10, 500u);
  // The in-memory state and published entity value still track every change; only the flash
  // write is held back.
  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.02f);
  EXPECT_FLOAT_EQ(gauge.soc_sensor_.state, 50.2f);
}

TEST_F(BatteryGaugeTest, PublishSavesOnceChangeExceedsThreshold) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.publish_(5.0f);
  ASSERT_EQ(gauge.persisted_.percentage_x10, 500u);

  gauge.publish_(5.6f);  // 56.0% -> a 60-tenths-of-a-percent move, well above the threshold
  EXPECT_EQ(gauge.persisted_.percentage_x10, 560u);
}

TEST_F(BatteryGaugeTest, PublishSavesSmallChangeAfterMinInterval) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.publish_(5.0f);
  ASSERT_EQ(gauge.persisted_.percentage_x10, 500u);
  // Pretend the last save happened long enough ago that the minimum interval has elapsed.
  gauge.last_saved_time_ = millis() - 6u * 60u * 1000u;

  gauge.publish_(5.02f);  // still only a 2-tenths-of-a-percent move
  EXPECT_EQ(gauge.persisted_.percentage_x10, 502u);
}

// --- ema_update(): time-based EMA filter ---

TEST_F(BatteryGaugeTest, EmaUpdateConvergesTowardRawValue) {
  // After one time constant, the filter should have moved ~63% of the way to the raw value.
  float result = TestableBatteryGauge::ema_update(0.0f, 10.0f, 30.0f, 30.0f);
  EXPECT_NEAR(result, 10.0f * (1.0f - std::exp(-1.0f)), 1e-4f);
}

TEST_F(BatteryGaugeTest, EmaUpdateZeroIntervalLeavesFilterUnchanged) {
  EXPECT_FLOAT_EQ(TestableBatteryGauge::ema_update(5.0f, 10.0f, 0.0f, 30.0f), 5.0f);
}

TEST_F(BatteryGaugeTest, EmaUpdateIsIntervalIndependent) {
  // Applying the filter in several small steps that sum to the same elapsed time as one big
  // step must produce the same result: exp(-a/tau)*exp(-b/tau)*exp(-c/tau) == exp(-(a+b+c)/tau).
  constexpr float tau = 30.0f;
  constexpr float raw = 10.0f;

  float single_step = TestableBatteryGauge::ema_update(0.0f, raw, 12.0f, tau);

  float multi_step = 0.0f;
  multi_step = TestableBatteryGauge::ema_update(multi_step, raw, 3.0f, tau);
  multi_step = TestableBatteryGauge::ema_update(multi_step, raw, 4.0f, tau);
  multi_step = TestableBatteryGauge::ema_update(multi_step, raw, 5.0f, tau);

  EXPECT_NEAR(single_step, multi_step, 1e-4f);
}

// --- on_current_() / on_voltage_(): the EMA filters use elapsed wall time, not sample count ---

TEST_F(BatteryGaugeTest, OnCurrentFilterConvergesQuicklyOverLongInterval) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.filtered_current_ = 0.0f;
  gauge.last_current_ = 0.0f;
  gauge.charge_state_ = 5.0f;
  // Five time constants ago: alpha ~= 1, so this single sample should nearly fully replace the
  // filtered value, regardless of how many samples would normally have arrived in between.
  gauge.last_time_ = millis() - 150000u;  // 150s = 5 * tau(30s)

  gauge.on_current_(10.0f);

  EXPECT_NEAR(gauge.filtered_current_, 10.0f, 0.1f);
}

TEST_F(BatteryGaugeTest, OnVoltageFilterConvergesQuicklyOverLongInterval) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.filtered_voltage_ = 3.0f;
  gauge.last_voltage_time_ = millis() - 150000u;  // 5 time constants ago

  gauge.on_voltage_(4.0f);

  EXPECT_NEAR(gauge.filtered_voltage_, 4.0f, 0.1f);
}

// --- on_current_(): integration of current over time ---

TEST_F(BatteryGaugeTest, OnCurrentIgnoresNonFinite) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.last_current_ = 1.0f;
  gauge.charge_state_ = 5.0f;

  gauge.on_current_(NAN);
  gauge.on_current_(INFINITY);

  // Invalid samples are dropped without touching any state.
  EXPECT_FLOAT_EQ(gauge.last_current_, 1.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

TEST_F(BatteryGaugeTest, OnCurrentFirstSampleEstablishesBaseline) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.charge_state_ = 5.0f;
  // last_time_ defaults to 0 -> first sample only records state, no integration.
  gauge.on_current_(2.0f);

  EXPECT_FLOAT_EQ(gauge.last_current_, 2.0f);
  EXPECT_FLOAT_EQ(gauge.filtered_current_, 2.0f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

TEST_F(BatteryGaugeTest, OnCurrentPositiveIncreasesCharge) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
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
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = -2.0f;
  gauge.filtered_current_ = -2.0f;
  gauge.last_time_ = millis() - ONE_HOUR_MS;

  gauge.on_current_(-2.0f);

  EXPECT_LT(gauge.charge_state_, 5.0f);
  EXPECT_GE(gauge.charge_state_, 0.0f);
}

// --- on_current_(): chemistry-specific derating (lead-acid) ---

TEST_F(BatteryGaugeTest, OnCurrentAppliesPeukertDeratingToDischarge) {
  // capacity=10Ah, capacity_rate=20h -> rated_current = 0.5A; discharging at 1A is 2x rated.
  LeadAcidChemistry chem(1.0f /* no acceptance falloff */, 1.25f, 0.5f, 0.04f, 0);
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 14.4f, &chem);
  gauge.charge_state_ = 10.0f;
  gauge.last_current_ = -1.0f;
  gauge.filtered_current_ = -1.0f;
  gauge.last_time_ = millis() - ONE_HOUR_MS;

  gauge.on_current_(-1.0f);

  // Naive integration over 1h at 1A would remove 1.0Ah. Peukert at 2x rated current with
  // k=1.25 removes 1.0 * 2^0.25 =~ 1.19Ah instead -> the gauge should read lower than the
  // naive result.
  EXPECT_LT(gauge.charge_state_, 9.0f);
  EXPECT_NEAR(gauge.charge_state_, 10.0f - std::pow(2.0f, 0.25f), 0.05f);
}

TEST_F(BatteryGaugeTest, OnCurrentAppliesChargeAcceptanceFalloffAboveKnee) {
  LeadAcidChemistry chem(0.8f, 1.0f /* no Peukert */, 0.5f, 0.04f, 0);
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 14.4f, &chem);
  gauge.charge_state_ = 9.0f;  // 90% SoC, above the 80% knee
  gauge.last_current_ = 1.0f;
  gauge.filtered_current_ = 1.0f;
  gauge.last_time_ = millis() - ONE_HOUR_MS;

  gauge.on_current_(1.0f);

  // acceptance(0.9) = (1-0.9)/(1-0.8) = 0.5, so only half the naive 1.0Ah is accepted.
  EXPECT_NEAR(gauge.charge_state_, 9.5f, 0.05f);
}

// --- on_voltage_(): full-charge resynchronisation ---

TEST_F(BatteryGaugeTest, OnVoltageFirstSampleSeedsFilter) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  // filtered_voltage_ starts as NAN; the first sample seeds it and returns.
  gauge.on_voltage_(3.7f);
  EXPECT_FLOAT_EQ(gauge.filtered_voltage_, 3.7f);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 0.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageFilterKeepsRunningUnderLoad) {
  // Unlike the old last_current_-gated early return, the voltage filter now keeps updating
  // during heavy charge/discharge; only the resync test itself is gated, on filtered_current_.
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.filtered_voltage_ = 3.7f;
  gauge.last_voltage_time_ = millis() - 150000u;  // long enough ago to fully converge
  gauge.last_current_ = 5.0f;                     // heavy load; irrelevant to filtering now
  gauge.filtered_current_ = 5.0f;                 // heavy load; this is what gates resync

  gauge.on_voltage_(4.2f);

  // Filter moved toward the new sample instead of being frozen.
  EXPECT_NEAR(gauge.filtered_voltage_, 4.2f, 0.1f);
  // But no resync: filtered_current_ is well above the tail-current threshold.
  EXPECT_FLOAT_EQ(gauge.charge_state_, 0.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageFullChargeResyncsToCapacity) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = 0.0f;
  gauge.filtered_voltage_ = 4.2f;
  gauge.filtered_current_ = 0.0f;  // below capacity * 0.02 (0.2 A)

  gauge.on_voltage_(4.2f);

  EXPECT_FLOAT_EQ(gauge.charge_state_, 10.0f);
  EXPECT_FLOAT_EQ(gauge.soc_sensor_.state, 100.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageNoResyncWhileStillCharging) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = 0.0f;
  gauge.filtered_voltage_ = 4.2f;
  // A significant charge current (>= capacity * 0.02) means "not full yet".
  gauge.filtered_current_ = 1.0f;

  gauge.on_voltage_(4.2f);

  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageNoResyncBelowMaxVoltage) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
  gauge.charge_state_ = 5.0f;
  gauge.last_current_ = 0.0f;
  gauge.filtered_voltage_ = 4.0f;
  gauge.filtered_current_ = 0.0f;

  gauge.on_voltage_(4.0f);

  EXPECT_FLOAT_EQ(gauge.charge_state_, 5.0f);
}

// --- on_voltage_(): full-charge dwell timer (lead-acid chemistries) ---

TEST_F(BatteryGaugeTest, OnVoltageLeadAcidDoesNotResyncBeforeDwellElapses) {
  LeadAcidChemistry chem(0.8f, 1.25f, 0.5f, 0.04f, 180000);  // 3-minute dwell
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 14.4f, &chem);
  gauge.charge_state_ = 9.0f;
  gauge.filtered_voltage_ = 14.4f;
  gauge.last_voltage_time_ = millis() - 150000u;
  gauge.filtered_current_ = 0.1f;  // below the 4% (0.4 A) tail

  gauge.on_voltage_(14.4f);

  // The full condition just became true; the 3-minute dwell hasn't elapsed yet.
  EXPECT_FLOAT_EQ(gauge.charge_state_, 9.0f);
  EXPECT_NE(gauge.full_condition_since_, 0u);
}

TEST_F(BatteryGaugeTest, OnVoltageLeadAcidResyncsOnceDwellElapses) {
  LeadAcidChemistry chem(0.8f, 1.25f, 0.5f, 0.04f, 180000);
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 14.4f, &chem);
  gauge.charge_state_ = 9.0f;
  gauge.filtered_voltage_ = 14.4f;
  gauge.filtered_current_ = 0.1f;
  gauge.last_voltage_time_ = millis() - 150000u;
  // Pretend the full condition has already held for longer than the dwell.
  gauge.full_condition_since_ = millis() - 200000u;

  gauge.on_voltage_(14.4f);

  EXPECT_FLOAT_EQ(gauge.charge_state_, 10.0f);
}

TEST_F(BatteryGaugeTest, OnVoltageLeadAcidDwellResetsWhenConditionDrops) {
  LeadAcidChemistry chem(0.8f, 1.25f, 0.5f, 0.04f, 180000);
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 14.4f, &chem);
  gauge.charge_state_ = 9.0f;
  gauge.filtered_voltage_ = 14.4f;
  gauge.last_voltage_time_ = millis() - 150000u;
  gauge.full_condition_since_ = millis() - 200000u;  // was already "full" a while ago
  gauge.filtered_current_ = 1.0f;                    // now above the tail threshold

  gauge.on_voltage_(14.4f);

  EXPECT_EQ(gauge.full_condition_since_, 0u);
  EXPECT_FLOAT_EQ(gauge.charge_state_, 9.0f);
}

// --- configuration setters ---

TEST_F(BatteryGaugeTest, SetInitialState) {
  TestableBatteryGauge gauge(&this->voltage_source_, &this->current_source_, 10.0f, 1.0f, 4.2f,
                             &this->lithium_chemistry_);
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
