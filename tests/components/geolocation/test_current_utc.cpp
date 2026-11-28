#include <gtest/gtest.h>

#include "esphome/components/geolocation/geolocation.h"

namespace esphome::geolocation::testing {

static GeolocationData received(time_t utc, uint64_t at_ms) {
  GeolocationData d{};
  d.utc = utc;
  d.utc_millis = at_ms;
  return d;
}

TEST(GeolocationCurrentUtc, NoUtcGivesZero) { EXPECT_EQ(current_utc(GeolocationData{}, 5000), 0); }

TEST(GeolocationCurrentUtc, AdvancedByWholeSecondsOfAge) {
  const auto d = received(1795608000, 1000);
  EXPECT_EQ(current_utc(d, 1000), 1795608000);
  EXPECT_EQ(current_utc(d, 1999), 1795608000);
  EXPECT_EQ(current_utc(d, 4500), 1795608003);
}

TEST(GeolocationCurrentUtc, StaleUtcIsIgnored) {
  const auto d = received(1795608000, 1000);
  EXPECT_EQ(current_utc(d, 1000 + MAX_UTC_AGE_MS), 1795608010);
  EXPECT_EQ(current_utc(d, 1001 + MAX_UTC_AGE_MS), 0);
}

TEST(GeolocationCurrentUtc, NoWrapAfter49Days) {
  // A 32-bit millis() age would wrap here and look fresh again
  const uint64_t received_at = 1000;
  const auto d = received(1795608000, received_at);
  EXPECT_EQ(current_utc(d, received_at + (1ULL << 32)), 0);
}

TEST(GeolocationCurrentUtc, FractionOfTheSecondCounts) {
  GeolocationData d{};
  d.millisecond = 900;  // 12:34:56.900 at 1000 ms
  set_utc(d, 1795608000, 1000);
  EXPECT_EQ(current_utc(d, 1099), 1795608000);
  EXPECT_EQ(current_utc(d, 1100), 1795608001);
  EXPECT_EQ(current_utc(d, 1200), 1795608001);
}

TEST(GeolocationFieldAge, SaturatesInsteadOfWrapping) {
  EXPECT_EQ(age_ms(1000, 3000), 2000u);
  EXPECT_EQ(age_ms(1000, 1000 + (1ULL << 32) + 1000), UINT32_MAX);
}

}  // namespace esphome::geolocation::testing
