#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/select/select.h"

namespace esphome::select::testing {

static constexpr const char *const APPLY_OPTIONS[] = {"low", "medium", "high"};

static std::vector<std::string> chosen;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_control(Select *entity, size_t index) { chosen.emplace_back(entity->option_at(index)); }

TEST(ApplySelect, ControlRunsTheFunctionWithTheIndex) {
  chosen.clear();
  ApplySelect<on_control> select;
  select.traits.set_options_static(APPLY_OPTIONS, 3);
  select.make_call().set_index(2).perform();
  select.make_call().set_option("low").perform();
  EXPECT_EQ(chosen, (std::vector<std::string>{"high", "low"}));
}

TEST(ApplySelect, HoldsNoMoreThanTheBase) { EXPECT_EQ(sizeof(ApplySelect<on_control>), sizeof(Select)); }

}  // namespace esphome::select::testing
