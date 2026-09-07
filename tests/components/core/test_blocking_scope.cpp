#include <gtest/gtest.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome {

// The scope must push the pass start forward by the time it covers and by
// nothing else, so the blocking guard sees only the work outside it
TEST(UnavoidableBlockingScope, ExcludesItsDurationFromThePass) {
  const uint32_t pass_start = millis();
  LoopBlockingGuard guard(nullptr, nullptr, pass_start);
  ASSERT_EQ(App.get_loop_component_start_time(), pass_start);

  const uint32_t before = millis();
  {
    UnavoidableBlockingScope scope;
    delay(30);
  }
  const uint32_t excused = millis() - before;

  const uint32_t moved = App.get_loop_component_start_time() - pass_start;
  EXPECT_GE(moved, 30u);
  EXPECT_LE(moved, excused);
}

TEST(UnavoidableBlockingScope, ZeroLengthScopeLeavesTheStartAlone) {
  const uint32_t pass_start = millis();
  LoopBlockingGuard guard(nullptr, nullptr, pass_start);
  { UnavoidableBlockingScope scope; }
  EXPECT_LE(App.get_loop_component_start_time() - pass_start, 1u);
}

}  // namespace esphome
