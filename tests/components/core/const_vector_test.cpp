#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>

#include "esphome/core/helpers.h"

namespace esphome::testing {

static constexpr const char *const TABLE[] = {"a", "b", "c"};

// Exposes the owned flag, which is protected.
class ProbeVector : public ConstVector<const char *, true> {
 public:
  using ConstVector::ConstVector;
  bool owned() const { return (this->size_ & OWNED_BIT) != 0; }
};

TEST(ConstVector, StaticTableIsViewedNotOwned) {
  ProbeVector list;
  EXPECT_TRUE(list.empty());
  list.assign_static(TABLE, 3);
  EXPECT_EQ(list.data(), TABLE);
  EXPECT_EQ(list.size(), 3U);
  EXPECT_FALSE(list.owned());
  EXPECT_STREQ(list[1], "b");
  EXPECT_STREQ(list.at(2), "c");
}

TEST(ConstVector, CopyIsOwnedAndSizeMasksTheFlag) {
  ProbeVector list;
  list.assign_copy(TABLE, 3);
  EXPECT_NE(list.data(), TABLE);
  EXPECT_TRUE(list.owned());
  EXPECT_EQ(list.size(), 3U);
  EXPECT_STREQ(list[0], "a");

  const char *const next[] = {"x", "y"};
  list.assign_copy(next, 2);  // frees the previous copy
  EXPECT_TRUE(list.owned());
  EXPECT_EQ(list.size(), 2U);
  EXPECT_STREQ(list[1], "y");
}

TEST(ConstVector, StaticThenRuntimeCopiesNeverFreeTheTable) {
  ProbeVector list;
  list.assign_static(TABLE, 3);
  EXPECT_FALSE(list.owned());
  list.assign_copy(TABLE, 2);  // the static table is not owned, so nothing is freed
  EXPECT_TRUE(list.owned());
  EXPECT_NE(list.data(), TABLE);
  const char *const next[] = {"x"};
  list.assign_copy(next, 1);  // frees the previous copy
  EXPECT_EQ(list.size(), 1U);
  EXPECT_STREQ(list[0], "x");
  EXPECT_STREQ(TABLE[0], "a");
}

TEST(ConstVector, EmptyCopyIsEmptyAndFreedOnNextSet) {
  ProbeVector list;
  list.assign_copy(TABLE, 3);
  list.assign_copy(TABLE, 0);
  EXPECT_TRUE(list.empty());
  list.assign_copy(TABLE, 2);  // frees the empty copy
  EXPECT_EQ(list.size(), 2U);
}

TEST(ConstVector, OwningVariantIsNotCopyable) {
  static_assert(!std::is_copy_constructible_v<ConstVector<const char *, true>>);
  static_assert(!std::is_copy_assignable_v<ConstVector<const char *, true>>);
}

TEST(ConstVector, CopyFromItsOwnStorage) {
  ProbeVector list;
  list.assign_copy(TABLE, 3);
  list.assign_copy(list.data(), list.size());
  EXPECT_EQ(list.size(), 3U);
  EXPECT_STREQ(list[2], "c");
}

TEST(ConstVector, PlainViewHasNoOwnershipCost) {
  static_assert(std::is_trivially_copyable_v<ConstVector<const char *>>);
  static_assert(std::is_trivially_destructible_v<ConstVector<const char *>>);
  ConstVector<const char *> list(TABLE, 3);
  EXPECT_EQ(list.size(), 3U);
  EXPECT_STREQ(list[2], "c");
}

TEST(ConstVector, IteratorsAreRawPointers) {
  ConstVector<const char *, true> list(TABLE, 3);
  static_assert(std::is_same_v<decltype(list.begin()), const char *const *>);
  const auto *it = std::find(list.begin(), list.end(), TABLE[1]);
  EXPECT_EQ(it - list.begin(), 1);
}

}  // namespace esphome::testing
