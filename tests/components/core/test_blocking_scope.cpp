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

// Nested scopes leave out the outer span exactly once, and never move the
// start past now
TEST(UnavoidableBlockingScope, NestedScopesExcuseTheOuterSpanOnce) {
  const uint32_t pass_start = millis();
  LoopBlockingGuard guard(nullptr, nullptr, pass_start);
  const uint32_t before = millis();
  {
    UnavoidableBlockingScope outer;
    {
      UnavoidableBlockingScope inner;
      delay(30);
    }
    delay(5);
  }
  const uint32_t excused = millis() - before;
  const uint32_t moved = App.get_loop_component_start_time() - pass_start;
  EXPECT_GE(moved, 35u);
  EXPECT_LE(moved, excused);
  EXPECT_GE(static_cast<int32_t>(millis() - App.get_loop_component_start_time()), 0);
}

namespace {
// Static: the guard publishes the component to App and nothing clears it.
// One instance per test, since a ratcheted threshold is permanent
class DummyComponent : public Component {};
DummyComponent &blocking_test_component(size_t index) {
  static DummyComponent components[2];
  return components[index];
}
}  // namespace

// The excused stretch must neither warn nor ratchet the component's threshold
TEST(UnavoidableBlockingScope, ExcusedStretchDoesNotRatchetTheThreshold) {
  DummyComponent &component = blocking_test_component(0);
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

// Work outside the scope is still measured and still ratchets
TEST(UnavoidableBlockingScope, WorkOutsideTheScopeStillRatchetsTheThreshold) {
  DummyComponent &component = blocking_test_component(1);
  uint32_t threshold_before = 0;
  component.should_warn_of_blocking(0, threshold_before);
  {
    LoopBlockingGuard guard(&component, nullptr, millis());
    {
      UnavoidableBlockingScope scope;
      delay(20);
    }
    delay(WARN_IF_BLOCKING_OVER_CS * 10U + 20);
    guard.finish();
  }
  uint32_t threshold_after = 0;
  component.should_warn_of_blocking(0, threshold_after);
  EXPECT_GT(threshold_after, threshold_before);
}

}  // namespace esphome
