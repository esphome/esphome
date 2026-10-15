#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/homeassistant/homeassistant.h"

namespace esphome::homeassistant::testing {

namespace {

struct Parsed {
  bool valid;
  std::vector<std::string> items;
};

Parsed parse(const char *text) {
  Parsed result;
  std::string item;
  result.valid = parse_python_list(
      StringRef(text), [&](char c) { item += c; },
      [&]() {
        result.items.push_back(item);
        item.clear();
      });
  return result;
}

}  // namespace

TEST(PythonList, ReadsStringsWithTheirEscapes) {
  auto parsed = parse("['Low', \"It's high\", 'caf\\xe9']");
  ASSERT_TRUE(parsed.valid);
  ASSERT_EQ(parsed.items.size(), 3u);
  EXPECT_EQ(parsed.items[0], "Low");
  EXPECT_EQ(parsed.items[1], "It's high");
  EXPECT_EQ(parsed.items[2], "caf\xc3\xa9");
}

TEST(PythonList, ReadsTheValueOfEnumMembers) {
  auto parsed = parse("[<ColorMode.HS: 'hs'>, <ColorMode.COLOR_TEMP: 'color_temp'>]");
  ASSERT_TRUE(parsed.valid);
  ASSERT_EQ(parsed.items.size(), 2u);
  EXPECT_EQ(parsed.items[0], "hs");
  EXPECT_EQ(parsed.items[1], "color_temp");
}

TEST(PythonList, AcceptsAnEmptyList) {
  auto parsed = parse("[]");
  EXPECT_TRUE(parsed.valid);
  EXPECT_TRUE(parsed.items.empty());
}

TEST(PythonList, RejectsMalformedLists) {
  for (const char *text : {"", "[", "['a'", "['a',", "['a' 'b']", "['a'] x", "[1]", "['a\\q']", "[<ColorMode.HS: 'hs'",
                           "[<ColorMode.HS>]"}) {
    EXPECT_FALSE(parse(text).valid) << text;
  }
}

}  // namespace esphome::homeassistant::testing
