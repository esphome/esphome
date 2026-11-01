#include <gtest/gtest.h>

#include "esphome/components/sgp30/sgp30.h"

namespace esphome::sgp30::testing {

// Reference values from the Buck equations in double precision; the ice and water
// coefficients differ by 1e-4, so the tolerance must stay well below that
TEST(SGP30CompensationHumidity, UsesTheIceCoefficientBelowZero) {
  EXPECT_NEAR(compensation_absolute_humidity(-10.0f, 50.0f), 1.0701634f, 0.00002f);
  EXPECT_NEAR(compensation_absolute_humidity(-30.0f, 50.0f), 0.1694181f, 0.000004f);
}

TEST(SGP30CompensationHumidity, UsesTheWaterCoefficientAboveZero) {
  EXPECT_NEAR(compensation_absolute_humidity(20.0f, 50.0f), 8.6414485f, 0.0002f);
}

}  // namespace esphome::sgp30::testing
