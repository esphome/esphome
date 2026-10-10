#include <gtest/gtest.h>

#include "esphome/core/string_ref.h"

#include <type_traits>
#include <utility>

namespace esphome::core::testing {

// ram_ref() must return a reference so callers can hand it out as `const StringRef &`
static_assert(std::is_same_v<decltype(std::declval<const ProgmemStringRef &>().ram_ref()), const StringRef &>);

TEST(ProgmemStringRef, WriteToCopiesAndTerminates) {
  ProgmemStringRef ref("Kitchen", 7);
  char buf[16];
  EXPECT_EQ(ref.write_to(buf, sizeof(buf)), 7u);
  EXPECT_STREQ(buf, "Kitchen");
}

TEST(ProgmemStringRef, WriteToTruncatesToFit) {
  ProgmemStringRef ref("Kitchen", 7);
  char buf[4];
  EXPECT_EQ(ref.write_to(buf, sizeof(buf)), 3u);
  EXPECT_STREQ(buf, "Kit");
}

TEST(ProgmemStringRef, WriteToZeroSizeWritesNothing) {
  ProgmemStringRef ref("Kitchen", 7);
  char buf[1] = {'x'};
  EXPECT_EQ(ref.write_to(buf, 0), 0u);
  EXPECT_EQ(buf[0], 'x');
}

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
