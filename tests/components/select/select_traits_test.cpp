#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/select/select_traits.h"

namespace esphome::select::testing {

static constexpr const char *const OPTIONS[] = {"low", "medium", "high"};

TEST(SelectTraits, ViewsTheTableWithoutCopying) {
  SelectTraits traits;
  EXPECT_TRUE(traits.get_options().empty());
  traits.set_options_static(OPTIONS, 3);
  const auto &options = traits.get_options();
  EXPECT_EQ(options.size(), 3U);
  EXPECT_FALSE(options.empty());
  EXPECT_EQ(options.data(), OPTIONS);
  EXPECT_STREQ(options[1], "medium");
  EXPECT_STREQ(options.at(2), "high");
  std::vector<std::string> seen;
  for (const char *option : options)
    seen.emplace_back(option);
  EXPECT_EQ(seen, (std::vector<std::string>{"low", "medium", "high"}));
}

TEST(SelectTraits, RuntimeListsAreCopied) {
  SelectTraits traits;
  traits.set_options({"a", "b"});
  EXPECT_EQ(traits.get_options().size(), 2U);
  EXPECT_STREQ(traits.get_options()[1], "b");

  FixedVector<const char *> list;
  list.init(3);
  list.push_back("x");
  list.push_back("y");
  list.push_back("z");
  traits.set_options(list);
  EXPECT_NE(traits.get_options().data(), list.begin());
  EXPECT_EQ(traits.get_options().size(), 3U);
  EXPECT_STREQ(traits.get_options().at(2), "z");

  // A later runtime list replaces the earlier copy
  traits.set_options({"only"});
  EXPECT_EQ(traits.get_options().size(), 1U);
  EXPECT_STREQ(traits.get_options()[0], "only");
}

TEST(SelectTraits, CopyingAnotherSelectSurvivesItsNextRuntimeList) {
  SelectTraits source;
  source.set_options({"a", "b"});
  SelectTraits copy;
  copy.set_options(source.get_options());
  EXPECT_NE(copy.get_options().data(), source.get_options().data());

  source.set_options({"c"});
  ASSERT_EQ(copy.get_options().size(), 2U);
  EXPECT_STREQ(copy.get_options()[0], "a");
  EXPECT_STREQ(copy.get_options()[1], "b");
}

TEST(SelectTraits, StaticTablesAreNeverOwned) {
  SelectTraits traits;
  traits.set_options_static(OPTIONS, 3);
  EXPECT_EQ(traits.get_options().data(), OPTIONS);
  EXPECT_EQ(traits.get_options().size(), 3U);
  // A runtime list after a static one copies and leaves the static table alone
  traits.set_options({"x"});
  EXPECT_NE(traits.get_options().data(), OPTIONS);
  EXPECT_EQ(traits.get_options().size(), 1U);
  EXPECT_STREQ(OPTIONS[0], "low");
}

TEST(SelectTraits, CopyOfItsOwnOptionsStaysValid) {
  SelectTraits traits;
  traits.set_options({"a", "b"});
  traits.set_options(traits.get_options());
  ASSERT_EQ(traits.get_options().size(), 2U);
  EXPECT_STREQ(traits.get_options()[1], "b");
}

}  // namespace esphome::select::testing
