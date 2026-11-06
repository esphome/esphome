#include <gtest/gtest.h>

#include "esphome/core/base_automation.h"

namespace esphome::core::testing {

namespace {

int calls = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void count_call() { calls++; }

// Bit patterns of 800.0f, -100.0f and 250.5f, as the code generator writes them
constexpr uint32_t PRIORITY_800 = 0x44480000;
constexpr uint32_t PRIORITY_MINUS_100 = 0xC2C80000;
constexpr uint32_t PRIORITY_250_5 = 0x437A8000;

}  // namespace

TEST(CoreAutomationCallbacks, StartupCallbackRunsFromSetupAtItsPriority) {
  calls = 0;
  StartupCallback<count_call, PRIORITY_800> high;
  StartupCallback<count_call, PRIORITY_MINUS_100> late;
  StartupCallback<count_call, PRIORITY_250_5> fractional;
  EXPECT_EQ(high.get_setup_priority(), 800.0f);
  EXPECT_EQ(late.get_setup_priority(), -100.0f);
  EXPECT_EQ(fractional.get_setup_priority(), 250.5f);

  high.loop();
  high.on_shutdown();
  EXPECT_EQ(calls, 0);
  high.setup();
  EXPECT_EQ(calls, 1);
}

TEST(CoreAutomationCallbacks, ShutdownCallbackRunsFromOnShutdownAtItsPriority) {
  calls = 0;
  ShutdownCallback<count_call, PRIORITY_MINUS_100> shutdown;
  EXPECT_EQ(shutdown.get_setup_priority(), -100.0f);

  shutdown.setup();
  shutdown.loop();
  EXPECT_EQ(calls, 0);
  shutdown.on_shutdown();
  EXPECT_EQ(calls, 1);
}

TEST(CoreAutomationCallbacks, LoopCallbackRunsEachLoopAtDataPriority) {
  calls = 0;
  LoopCallback<count_call> looping;
  EXPECT_EQ(looping.get_setup_priority(), setup_priority::DATA);
  EXPECT_TRUE(HasLoopOverride<LoopCallback<count_call>>::value);
  EXPECT_FALSE((HasLoopOverride<StartupCallback<count_call, PRIORITY_800>>::value));

  looping.setup();
  EXPECT_EQ(calls, 0);
  looping.loop();
  looping.loop();
  EXPECT_EQ(calls, 2);
}

}  // namespace esphome::core::testing
