#include <gtest/gtest.h>

#include "esphome/core/string_ref.h"

namespace esphome::core::testing {

TEST(ProgmemStringRef, EqualsComparesLengthAndContent) {
  ProgmemStringRef ref("Kitchen", 7);
  EXPECT_TRUE(ref.equals(StringRef("Kitchen")));
  EXPECT_FALSE(ref.equals(StringRef("Kitche")));
  EXPECT_FALSE(ref.equals(StringRef("Kitchens")));
  EXPECT_FALSE(ref.equals(StringRef("Bedroom")));
}

TEST(ProgmemStringRef, DefaultIsEmpty) {
  ProgmemStringRef ref;
  EXPECT_TRUE(ref.empty());
  EXPECT_EQ(ref.size(), 0u);
  EXPECT_TRUE(ref.equals(StringRef("")));
}

}  // namespace esphome::core::testing
