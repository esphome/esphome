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

// The generated api messages start their encode only string fields as a null pointer with zero
// length; every member must treat that exactly like the default constructed empty string.
TEST(StringRefNullEmpty, BehavesAsEmptyString) {
  const StringRef null_empty{nullptr, 0};
  const StringRef empty;
  EXPECT_TRUE(null_empty.empty());
  EXPECT_EQ(null_empty.size(), 0u);
  EXPECT_EQ(null_empty.c_str(), nullptr);
  EXPECT_TRUE(null_empty == empty);
  EXPECT_TRUE(null_empty == "");
  EXPECT_TRUE(null_empty == std::string());
  EXPECT_EQ(null_empty.compare(empty), 0);
  EXPECT_EQ(null_empty.compare(""), 0);
  EXPECT_LT(null_empty.compare("a"), 0);
  EXPECT_TRUE(null_empty.starts_with(""));
  EXPECT_FALSE(null_empty.starts_with("a"));
  EXPECT_EQ(null_empty.str(), std::string());
  EXPECT_EQ(null_empty.substr(0), std::string());
  EXPECT_EQ(null_empty.find('a'), std::string::npos);
  EXPECT_EQ(null_empty.find("a"), std::string::npos);
  char buf[4] = "xyz";
  EXPECT_EQ(null_empty.copy(buf, sizeof(buf)), 0u);
  EXPECT_EQ(null_empty.begin(), null_empty.end());
}

TEST(StringRefNullEmpty, ComparesAgainstText) {
  const StringRef null_empty{nullptr, 0};
  const StringRef text("abc", 3);
  EXPECT_FALSE(null_empty == text);
  EXPECT_FALSE(text == null_empty);
  EXPECT_LT(null_empty.compare(text), 0);
  EXPECT_GT(text.compare(null_empty), 0);
  EXPECT_TRUE(text.starts_with(null_empty));
}

}  // namespace esphome::core::testing
