#include <gtest/gtest.h>
#include <cmath>
#include "esphome/components/battery_gauge/lead_acid_chemistry.h"

namespace esphome::battery_gauge::testing {

// capacity=10Ah, capacity_rate=20h -> rated_current = 0.5A
static constexpr float RATED_CURRENT = 0.5f;

// --- charge_acceptance(): falls off linearly above the knee ---

TEST(LeadAcidChemistryTest, ChargeAcceptanceIsFullBelowKnee) {
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 0);
  EXPECT_FLOAT_EQ(chem.charge_acceptance(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(chem.charge_acceptance(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(chem.charge_acceptance(0.8f), 1.0f);
}

TEST(LeadAcidChemistryTest, ChargeAcceptanceFallsOffAboveKnee) {
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 0);
  // Halfway between the knee (0.8) and full (1.0) -> half acceptance.
  EXPECT_NEAR(chem.charge_acceptance(0.9f), 0.5f, 1e-5f);
  // At full charge, acceptance has fallen to zero.
  EXPECT_NEAR(chem.charge_acceptance(1.0f), 0.0f, 1e-5f);
}

TEST(LeadAcidChemistryTest, ChargeAcceptanceNoOpWhenKneeIsOne) {
  // knee=100% is the "custom, no falloff configured" default: always full acceptance.
  LeadAcidChemistry chem(1.0f, 1.0f, RATED_CURRENT, 0.02f, 0);
  EXPECT_FLOAT_EQ(chem.charge_acceptance(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(chem.charge_acceptance(1.0f), 1.0f);
}

// --- discharge_scale(): Peukert derating ---

TEST(LeadAcidChemistryTest, DischargeScaleNoOpAtExponentOne) {
  LeadAcidChemistry chem(0.8f, 1.0f, RATED_CURRENT, 0.04f, 0);
  EXPECT_FLOAT_EQ(chem.discharge_scale(-5.0f), 1.0f);
  EXPECT_FLOAT_EQ(chem.discharge_scale(-0.1f), 1.0f);
}

TEST(LeadAcidChemistryTest, DischargeScaleNoOpAtRatedCurrent) {
  // (I / I_rated)^(k-1) == 1 whenever |I| == I_rated, regardless of k.
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 0);
  EXPECT_NEAR(chem.discharge_scale(-RATED_CURRENT), 1.0f, 1e-5f);
}

TEST(LeadAcidChemistryTest, DischargeScaleIncreasesAboveRatedCurrent) {
  // Drawing more than the rated current consumes effective capacity faster than the naive
  // integral would suggest.
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 0);
  float scale = chem.discharge_scale(-2.0f * RATED_CURRENT);
  EXPECT_GT(scale, 1.0f);
  EXPECT_NEAR(scale, std::pow(2.0f, 0.25f), 1e-4f);
}

TEST(LeadAcidChemistryTest, DischargeScaleDecreasesBelowRatedCurrent) {
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 0);
  float scale = chem.discharge_scale(-0.5f * RATED_CURRENT);
  EXPECT_LT(scale, 1.0f);
  EXPECT_NEAR(scale, std::pow(0.5f, 0.25f), 1e-4f);
}

// --- is_full(): configurable tail current ---

TEST(LeadAcidChemistryTest, IsFullUsesConfiguredTailCurrent) {
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 0);
  GaugeState state{.filtered_voltage = 14.4f,
                   .filtered_current = 0.3f,  // 3% of 10Ah capacity: below the 4% tail
                   .charge_state = 9.0f,
                   .capacity = 10.0f,
                   .max_charge_voltage = 14.4f};
  EXPECT_TRUE(chem.is_full(state));

  state.filtered_current = 0.5f;  // 5%: above the 4% tail
  EXPECT_FALSE(chem.is_full(state));
}

TEST(LeadAcidChemistryTest, IsFullRequiresVoltageAtOrAboveMax) {
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 0);
  GaugeState state{.filtered_voltage = 14.0f,
                   .filtered_current = 0.0f,
                   .charge_state = 9.0f,
                   .capacity = 10.0f,
                   .max_charge_voltage = 14.4f};
  EXPECT_FALSE(chem.is_full(state));
}

// --- full_charge_dwell_ms(): plumbed straight through ---

TEST(LeadAcidChemistryTest, FullChargeDwellMsReturnsConfiguredValue) {
  LeadAcidChemistry chem(0.8f, 1.25f, RATED_CURRENT, 0.04f, 180000);
  EXPECT_EQ(chem.full_charge_dwell_ms(), 180000u);
}

}  // namespace esphome::battery_gauge::testing
