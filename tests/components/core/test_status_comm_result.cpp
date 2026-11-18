#include <gtest/gtest.h>

#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {

TEST(StatusCommResult, FailureSetsWarningAndSuccessClearsIt) {
  Component component;
  EXPECT_FALSE(component.status_comm_result(false));
  EXPECT_TRUE(component.status_has_warning());
  EXPECT_TRUE(component.status_comm_result(true));
  EXPECT_FALSE(component.status_has_warning());
}

TEST(StatusCommResult, CustomMessageOverload) {
  Component component;
  EXPECT_FALSE(component.status_comm_result(false, LOG_STR("Bad CRC")));
  EXPECT_TRUE(component.status_has_warning());
  EXPECT_TRUE(component.status_comm_result(true, LOG_STR("Bad CRC")));
  EXPECT_FALSE(component.status_has_warning());
}

TEST(StatusCommResult, SuccessWithoutWarningStaysClear) {
  Component component;
  EXPECT_TRUE(component.status_comm_result(true));
  EXPECT_FALSE(component.status_has_warning());
}

}  // namespace esphome
