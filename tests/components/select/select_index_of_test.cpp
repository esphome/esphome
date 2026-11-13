#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "esphome/components/select/select.h"

namespace esphome::select::testing {

class IndexOfSelect : public Select {
 protected:
  void control(size_t index) override {}
};

static constexpr const char *const OPTIONS[] = {"still", "", "still+move", "s", "move", "off", "Off"};
static constexpr size_t OPTION_COUNT = sizeof(OPTIONS) / sizeof(OPTIONS[0]);

// An option matches when it has exactly `len` bytes and they are the same
static optional<size_t> reference_index_of(const char *option, size_t len) {
  for (size_t i = 0; i < OPTION_COUNT; i++) {
    if (strlen(OPTIONS[i]) == len && memcmp(OPTIONS[i], option, len) == 0)
      return i;
  }
  return {};
}

// Every option, each of its prefixes and each option with a byte added, read from a buffer whose following bytes
// are not a terminator, so index_of() must stay within `len`
TEST(SelectIndexOf, MatchesReferenceForPrefixesAndExtensions) {
  IndexOfSelect select;
  select.traits.set_options_static(OPTIONS, OPTION_COUNT);
  for (const char *opt : OPTIONS) {
    const std::string base(opt);
    for (const std::string &text : {base, base + "+", base + "e", base + "X"}) {
      const std::string padded = text + "still";  // bytes after `len` that would match if they were read
      for (size_t len = 0; len <= text.size(); len++) {
        EXPECT_EQ(select.index_of(padded.data(), len), reference_index_of(padded.data(), len))
            << "text '" << text.substr(0, len) << "'";
      }
    }
  }
}

TEST(SelectIndexOf, EmptyInputFindsOnlyTheEmptyOption) {
  IndexOfSelect select;
  select.traits.set_options_static(OPTIONS, OPTION_COUNT);
  EXPECT_EQ(select.index_of("x", 0), optional<size_t>(1));

  static constexpr const char *const NO_EMPTY[] = {"a", "b"};
  IndexOfSelect other;
  other.traits.set_options_static(NO_EMPTY, 2);
  EXPECT_FALSE(other.index_of("a", 0).has_value());
}

}  // namespace esphome::select::testing
