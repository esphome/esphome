#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/select/select_traits.h"

namespace esphome::select::testing {

static constexpr const char *const OPTIONS[] = {"low", "medium", "high"};

TEST(SelectTraits, ViewsTheTableWithoutCopying) {
  SelectTraits traits;
  EXPECT_TRUE(traits.get_options().empty());
  traits.set_options(OPTIONS, 3);
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

}  // namespace esphome::select::testing
