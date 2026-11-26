#include <gtest/gtest.h>

#include "esphome/core/automation.h"

// Globals named like the change helpers, declared the way main.cpp declares YAML ids: at global scope,
// next to `using namespace esphome;`. This file only compiles if the two stay apart.
static int left = 1;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int entered = 2;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int changed = 3;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

using namespace esphome;  // NOLINT(google-build-using-namespace)

TEST(ChangeTriggerNames, YamlIdsNamedLikeTheHelpersStayUsable) {
  bool last = false;
  // The form the generated filters use
  EXPECT_TRUE(esphome::internal::change_trigger::entered(last, true));
  EXPECT_TRUE(esphome::internal::change_trigger::left(last, false));
  int value = 0;
  EXPECT_TRUE(esphome::internal::change_trigger::changed(value, 1));
  // Unqualified, as a lambda names an id
  EXPECT_EQ(left + entered + changed, 6);
}
