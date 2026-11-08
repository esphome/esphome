#include <gtest/gtest.h>

#include "esphome/components/button/button.h"

namespace esphome::button::testing {

static Button *last_entity = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int presses = 0;                // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_press(Button *entity) {
  last_entity = entity;
  presses++;
}

TEST(ApplyButton, PressRunsTheFunctionWithTheEntity) {
  last_entity = nullptr;
  presses = 0;
  ApplyButton<on_press> button;
  button.press();
  button.press();
  EXPECT_EQ(presses, 2);
  EXPECT_EQ(last_entity, &button);
}

TEST(ApplyButton, HoldsNoMoreThanTheBase) { EXPECT_EQ(sizeof(ApplyButton<on_press>), sizeof(Button)); }

}  // namespace esphome::button::testing
