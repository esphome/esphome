#include <gtest/gtest.h>

#include "esphome/core/string_ref.h"

namespace esphome::core::testing {

TEST(StringRefStartsWith, ProperPrefixMatches) {
  StringRef ref("FR:R20:12345", 12);
  EXPECT_TRUE(ref.starts_with("FR:"));
}

TEST(StringRefStartsWith, WholeStringIsAPrefixOfItself) {
  StringRef ref("TP96", 4);
  EXPECT_TRUE(ref.starts_with("TP96"));
}

TEST(StringRefStartsWith, PrefixLongerThanViewFails) {
  StringRef ref("TP", 2);
  EXPECT_FALSE(ref.starts_with("TP96"));
}

TEST(StringRefStartsWith, DifferentContentFails) {
  StringRef ref("TP96", 4);
  EXPECT_FALSE(ref.starts_with("FR:"));
}

TEST(StringRefStartsWith, EmptyPrefixAlwaysMatches) {
  StringRef ref("abc", 3);
  EXPECT_TRUE(ref.starts_with(""));
  StringRef empty;
  EXPECT_TRUE(empty.starts_with(""));
}

TEST(StringRefStartsWith, EmptyViewOnlyMatchesEmptyPrefix) {
  StringRef empty;
  EXPECT_FALSE(empty.starts_with("a"));
}

TEST(StringRefStartsWith, WorksOnANonTerminatedBuffer) {
  // The reason the helper exists: a bounded view over a buffer with no
  // terminator anywhere near the viewed bytes.
  const char raw[] = {'R', 'a', 'd', 'o', 'n', 'X'};
  StringRef ref(raw, 5);
  EXPECT_TRUE(ref.starts_with("Radon"));
  EXPECT_FALSE(ref.starts_with("RadonEye"));
  EXPECT_FALSE(ref.starts_with("adon"));
}

TEST(StringRefStartsWith, StdStringOverload) {
  StringRef ref("TP96", 4);
  EXPECT_TRUE(ref.starts_with(std::string("TP")));
  EXPECT_FALSE(ref.starts_with(std::string("96")));
}

TEST(StringRefStartsWith, RefOverloadComparesOnlyTheViewedLength) {
  // The prefix is a bounded view: bytes past its length must not be compared.
  StringRef ref("FR:123", 6);
  StringRef prefix("FR:xyz", 3);
  EXPECT_TRUE(ref.starts_with(prefix));
}

}  // namespace esphome::core::testing
