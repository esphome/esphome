#include <cmath>

#include "gtest/gtest.h"
#include "esphome/components/max31855/max31855_linearize.h"

#ifdef USE_MAX31855_LINEARIZE

namespace esphome::max31855::testing {

namespace {

/// Reference points from the NIST ITS-90 type K table (NIST Monograph 175), mV.
struct NistPoint {
  float t;
  float mv;
};
constexpr NistPoint NIST_TABLE[] = {
    {-200.0f, -5.891f}, {-100.0f, -3.554f}, {-20.0f, -0.778f}, {0.0f, 0.0f},       {22.0f, 0.879f},
    {25.0f, 1.000f},    {100.0f, 4.096f},   {500.0f, 20.644f}, {1000.0f, 41.276f}, {1300.0f, 52.410f},
};

/// What the MAX31855 reports for a probe at t with its cold junction at t_cj:
/// t_cj plus the true thermocouple voltage divided by its fixed 41.276 uV/°C.
float chip_reading(float e_t, float t_cj, float e_cj) { return t_cj + (e_t - e_cj) / 0.041276f; }

}  // namespace

// The table is rounded to 1 uV, so allow 1.5 uV.
TEST(MAX31855Linearize, ForwardMatchesNistTable) {
  for (const auto &p : NIST_TABLE)
    EXPECT_NEAR(k_type_mv(p.t), p.mv, 0.0015f) << "T=" << p.t;
}

// NIST quotes the inverse polynomials to within 0.06 °C over their range. The
// table voltages are rounded to 1 uV, which adds up to 0.03 °C near -200 °C,
// where the curve is flattest.
TEST(MAX31855Linearize, InverseMatchesNistTable) {
  for (const auto &p : NIST_TABLE)
    EXPECT_NEAR(k_type_celsius(p.mv), p.t, 0.1f) << "E=" << p.mv;
}

TEST(MAX31855Linearize, CorrectsChipReading) {
  struct Case {
    float t, e_t, t_cj, e_cj;
  };
  constexpr Case CASES[] = {
      {-200.0f, -5.891f, 25.0f, 1.000f},  // chip reports about -142 °C
      {-20.0f, -0.778f, 22.0f, 0.879f},   // chip reports about -18 °C
      {500.0f, 20.644f, 25.0f, 1.000f},   // chip reports about 501 °C
      {1300.0f, 52.410f, 25.0f, 1.000f},  // chip reports about 1271 °C
  };
  for (const auto &c : CASES) {
    const float t_chip = chip_reading(c.e_t, c.t_cj, c.e_cj);
    EXPECT_GT(std::fabs(t_chip - c.t), 0.5f) << "case should need a correction, T=" << c.t;
    EXPECT_NEAR(k_type_linearize(t_chip, c.t_cj), c.t, 0.1f) << "T=" << c.t;
  }
}

TEST(MAX31855Linearize, BranchBoundaries) {
  EXPECT_NEAR(k_type_celsius(0.0f), 0.0f, 0.01f);
  EXPECT_NEAR(k_type_celsius(-0.0001f), k_type_celsius(0.0001f), 0.01f);
  EXPECT_NEAR(k_type_celsius(20.6439f), 500.0f, 0.06f);
  EXPECT_NEAR(k_type_celsius(20.644f), 500.0f, 0.06f);
  // At its exact cold-junction temperature the probe reads the same with or without correction.
  EXPECT_NEAR(k_type_linearize(25.0f, 25.0f), 25.0f, 0.05f);
}

TEST(MAX31855Linearize, OutsideNistRangeIsNan) {
  EXPECT_FALSE(std::isnan(k_type_celsius(K_TYPE_MV_MIN)));
  EXPECT_FALSE(std::isnan(k_type_celsius(K_TYPE_MV_MAX)));
  EXPECT_TRUE(std::isnan(k_type_celsius(-6.0f)));
  EXPECT_TRUE(std::isnan(k_type_celsius(55.0f)));
  EXPECT_TRUE(std::isnan(k_type_celsius(NAN)));
  // A chip reading of 1500 °C (cold junction 25 °C) is past the 1372 °C end of type K.
  EXPECT_TRUE(std::isnan(k_type_linearize(1500.0f, 25.0f)));
  // So is -250 °C, below the -200 °C end of the inverse tables.
  EXPECT_TRUE(std::isnan(k_type_linearize(-250.0f, 25.0f)));
}

}  // namespace esphome::max31855::testing

#endif  // USE_MAX31855_LINEARIZE
