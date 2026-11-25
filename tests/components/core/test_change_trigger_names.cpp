#include <gtest/gtest.h>

#include "esphome/core/automation.h"

// Globals named like the change helpers, declared the way main.cpp declares YAML ids: at global scope,
// next to `using namespace esphome;`. This file only compiles if the two stay apart. An id named change_trigger
// is rejected as reserved instead.
static int left = 1;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int entered = 2;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int changed = 3;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

using namespace esphome;  // NOLINT(google-build-using-namespace)

TEST(ChangeTriggerNames, YamlIdsNamedLikeTheHelpersStayUsable) {
  bool last = false;
  // The form the generated filters use
  EXPECT_TRUE(esphome::change_trigger::entered(last, true));
  EXPECT_TRUE(esphome::change_trigger::left(last, false));
  int value = 0;
  EXPECT_TRUE(esphome::change_trigger::changed(value, 1));
  // Unqualified, as a lambda names an id
  EXPECT_EQ(left + entered + changed, 6);
}

TEST(ChangeTriggerNames, QualifiedCallsReachTheHelpersPastTheGlobals) {
  // The globals hide nothing the generated code names: each call resolves through esphome::change_trigger
  bool last = true;
  EXPECT_FALSE(esphome::change_trigger::entered(last, true));
  EXPECT_FALSE(esphome::change_trigger::left(last, true));
  EXPECT_TRUE(esphome::change_trigger::left(last, false));
  const char *text = nullptr;
  EXPECT_TRUE(esphome::change_trigger::changed(text, "eco"));
  EXPECT_FALSE(esphome::change_trigger::changed(text, "eco"));
  EXPECT_EQ(left, 1);
  EXPECT_EQ(entered, 2);
  EXPECT_EQ(changed, 3);
}
