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
  const uint32_t before = millis();
  { UnavoidableBlockingScope scope; }
  EXPECT_LE(App.get_loop_component_start_time() - pass_start, millis() - before);
}

// Nesting counts the inner stretch twice; the start must still never pass now
TEST(UnavoidableBlockingScope, NestedScopesNeverMoveTheStartPastNow) {
  const uint32_t pass_start = millis();
  LoopBlockingGuard guard(nullptr, nullptr, pass_start);
  {
    UnavoidableBlockingScope outer;
    {
      UnavoidableBlockingScope inner;
      delay(30);
    }
    delay(5);
  }
  const uint32_t now = millis();
  EXPECT_GE(static_cast<int32_t>(now - App.get_loop_component_start_time()), 0);
  EXPECT_GE(App.get_loop_component_start_time() - pass_start, 35u);
}

class DummyComponent : public Component {};

// The excused stretch must neither warn nor ratchet the component's threshold
TEST(UnavoidableBlockingScope, ExcusedStretchDoesNotRatchetTheThreshold) {
  DummyComponent component;
  uint32_t threshold_before = 0;
  component.should_warn_of_blocking(0, threshold_before);
  {
    LoopBlockingGuard guard(&component, nullptr, millis());
    {
      UnavoidableBlockingScope scope;
      delay(WARN_IF_BLOCKING_OVER_CS * 10U + 20);
    }
    guard.finish();
  }
  uint32_t threshold_after = 0;
  component.should_warn_of_blocking(0, threshold_after);
  EXPECT_EQ(threshold_after, threshold_before);
}

}  // namespace esphome
