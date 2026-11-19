#include <gtest/gtest.h>

#include "esphome/components/ota/ota_backend.h"

namespace esphome::ota::testing {

// version_is_older(candidate, reference) == true means candidate is a downgrade
// and should be rejected.

TEST(VersionIsOlder, PatchOlder) {
  EXPECT_TRUE(version_is_older("1.2.3", "1.2.4"));
  EXPECT_FALSE(version_is_older("1.2.4", "1.2.3"));
}

TEST(VersionIsOlder, NumericNotLexical) {
  // "1.10.0" is newer than "1.9.0" even though '1' < '9' lexically.
  EXPECT_TRUE(version_is_older("1.9.0", "1.10.0"));
  EXPECT_FALSE(version_is_older("1.10.0", "1.9.0"));
}

TEST(VersionIsOlder, MajorMinor) {
  EXPECT_TRUE(version_is_older("1.9.9", "2.0.0"));
  EXPECT_TRUE(version_is_older("1.2.9", "1.3.0"));
  EXPECT_FALSE(version_is_older("2.0.0", "1.9.9"));
}

TEST(VersionIsOlder, EqualVersionsAllowed) {
  // Re-flashing the same version must be permitted.
  EXPECT_FALSE(version_is_older("1.2.3", "1.2.3"));
  EXPECT_FALSE(version_is_older("2024.1.0", "2024.1.0"));
}

TEST(VersionIsOlder, DifferingComponentCounts) {
  // Missing trailing components count as 0.
  EXPECT_FALSE(version_is_older("1.2", "1.2.0"));
  EXPECT_FALSE(version_is_older("1.2.0", "1.2"));
  EXPECT_TRUE(version_is_older("1.2", "1.2.1"));
  EXPECT_FALSE(version_is_older("1.2.1", "1.2"));
}

TEST(VersionIsOlder, CalendarVersions) {
  EXPECT_TRUE(version_is_older("2024.12.0", "2025.1.0"));
  EXPECT_FALSE(version_is_older("2025.1.0", "2024.12.0"));
}

TEST(VersionIsOlder, NullInputsAreSafe) {
  EXPECT_FALSE(version_is_older(nullptr, "1.2.3"));
  EXPECT_FALSE(version_is_older("1.2.3", nullptr));
  EXPECT_FALSE(version_is_older(nullptr, nullptr));
}

}  // namespace esphome::ota::testing
