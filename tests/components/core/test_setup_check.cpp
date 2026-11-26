#include <gtest/gtest.h>

#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {

TEST(SetupCheck, FailureMarksFailed) {
  Component component;
  EXPECT_FALSE(component.setup_check(false));
  EXPECT_TRUE(component.is_failed());
  EXPECT_TRUE(component.status_has_error());
}

TEST(SetupCheck, CustomMessageOverloadMarksFailed) {
  Component component;
  EXPECT_FALSE(component.setup_check(false, LOG_STR("Wrong chip ID")));
  EXPECT_TRUE(component.is_failed());
  EXPECT_TRUE(component.status_has_error());
}

TEST(SetupCheck, SuccessLeavesComponentUntouched) {
  Component component;
  EXPECT_TRUE(component.setup_check(true));
  EXPECT_TRUE(component.setup_check(true, LOG_STR("Wrong chip ID")));
  EXPECT_FALSE(component.is_failed());
  EXPECT_FALSE(component.status_has_error());
}

}  // namespace esphome
