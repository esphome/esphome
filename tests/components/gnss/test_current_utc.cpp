#include <gtest/gtest.h>

#include "esphome/components/gnss/gnss.h"

namespace esphome::gnss::testing {

static GNSSData received(time_t utc, uint64_t at_ms) {
  GNSSData d{};
  d.utc = utc;
  d.utc_millis = at_ms;
  return d;
}

TEST(GNSSCurrentUtc, NoUtcGivesZero) { EXPECT_EQ(current_utc(GNSSData{}, 5000), 0); }

TEST(GNSSCurrentUtc, AdvancedByWholeSecondsOfAge) {
  const auto d = received(1795608000, 1000);
  EXPECT_EQ(current_utc(d, 1000), 1795608000);
  EXPECT_EQ(current_utc(d, 1999), 1795608000);
  EXPECT_EQ(current_utc(d, 4500), 1795608003);
}

TEST(GNSSCurrentUtc, StaleUtcIsIgnored) {
  const auto d = received(1795608000, 1000);
  EXPECT_EQ(current_utc(d, 1000 + MAX_UTC_AGE_MS), 1795608010);
  EXPECT_EQ(current_utc(d, 1001 + MAX_UTC_AGE_MS), 0);
}

TEST(GNSSCurrentUtc, NoWrapAfter49Days) {
  // A 32-bit millis() age would wrap here and look fresh again
  const uint64_t received_at = 1000;
  const auto d = received(1795608000, received_at);
  EXPECT_EQ(current_utc(d, received_at + (1ULL << 32)), 0);
}

}  // namespace esphome::gnss::testing
