#include <gtest/gtest.h>

#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {

TEST(SetupCommResult, FailureMarksFailed) {
  Component component;
  EXPECT_FALSE(component.setup_comm_result(false));
  EXPECT_TRUE(component.is_failed());
  EXPECT_TRUE(component.status_has_error());
}

TEST(SetupCommResult, CustomMessageOverloadMarksFailed) {
  Component component;
  EXPECT_FALSE(component.setup_comm_result(false, LOG_STR("Wrong chip ID")));
  EXPECT_TRUE(component.is_failed());
  EXPECT_TRUE(component.status_has_error());
}

TEST(SetupCommResult, SuccessLeavesComponentUntouched) {
  Component component;
  EXPECT_TRUE(component.setup_comm_result(true));
  EXPECT_TRUE(component.setup_comm_result(true, LOG_STR("Wrong chip ID")));
  EXPECT_FALSE(component.is_failed());
  EXPECT_FALSE(component.status_has_error());
}

}  // namespace esphome
