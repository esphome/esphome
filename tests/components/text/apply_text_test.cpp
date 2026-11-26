#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/text/text.h"

namespace esphome::text::testing {

static Text *last_entity = nullptr;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<std::string> values;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_control(Text *entity, const std::string &value) {
  last_entity = entity;
  values.push_back(value);
}

TEST(ApplyText, ControlRunsTheFunctionWithTheValue) {
  last_entity = nullptr;
  values.clear();
  ApplyText<on_control> text;
  text.traits.set_min_length(0);
  text.traits.set_max_length(8);
  text.make_call().set_value("hello").perform();
  EXPECT_EQ(values, (std::vector<std::string>{"hello"}));
  EXPECT_EQ(last_entity, &text);
}

TEST(ApplyText, HoldsNoMoreThanTheBase) { EXPECT_EQ(sizeof(ApplyText<on_control>), sizeof(Text)); }

}  // namespace esphome::text::testing
