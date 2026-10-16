#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/switch/switch.h"

namespace esphome::switch_::testing {

static Switch *last_entity = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<bool> writes;       // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_write(Switch *entity, bool state) {
  last_entity = entity;
  writes.push_back(state);
}

TEST(ApplySwitch, WriteRunsTheFunctionWithTheState) {
  last_entity = nullptr;
  writes.clear();
  ApplySwitch<on_write> sw;
  sw.turn_on();
  sw.turn_off();
  EXPECT_EQ(writes, (std::vector<bool>{true, false}));
  EXPECT_EQ(last_entity, &sw);
}

TEST(ApplySwitch, InversionIsAppliedBeforeTheFunction) {
  writes.clear();
  ApplySwitch<on_write> sw;
  sw.set_inverted(true);
  sw.turn_on();
  EXPECT_EQ(writes, (std::vector<bool>{false}));
}

TEST(ApplySwitch, HoldsNoMoreThanTheBase) { EXPECT_EQ(sizeof(ApplySwitch<on_write>), sizeof(Switch)); }

}  // namespace esphome::switch_::testing
