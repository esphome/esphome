#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::binary_sensor::testing {

struct Transition {
  bool state;
  uint32_t at;
};

static void run_scheduler_for(uint32_t ms) {
  const uint32_t start = millis();
  while (millis() - start < ms) {
    delay(1);
    App.scheduler.call(millis());
  }
}

// Each timing step's delay, time_off and time_on are used in order, then the last step repeats
TEST(BinarySensorAutorepeat, StepsThroughEveryTiming) {
  BinarySensor sensor;
  AutorepeatFilter<2> autorepeat({{100, 40, 60}, {270, 120, 180}});
  sensor.add_filter(&autorepeat);
  std::vector<Transition> transitions;
  sensor.add_on_state_callback([&transitions](bool state) { transitions.push_back({state, millis()}); });

  sensor.publish_state(true);
  run_scheduler_for(760);

  // On for the first delay, then off/on from the first step, then from the second step after 100 + 270 ms
  static constexpr uint32_t EXPECTED_MS[] = {100, 40, 60, 40, 60, 40, 60, 120, 180};
  static constexpr size_t EXPECTED_COUNT = sizeof(EXPECTED_MS) / sizeof(EXPECTED_MS[0]);
  ASSERT_EQ(transitions.size(), EXPECTED_COUNT + 1);
  for (size_t i = 0; i < EXPECTED_COUNT; i++) {
    EXPECT_EQ(transitions[i].state, i % 2 == 0) << "transition " << i;
    EXPECT_NEAR(transitions[i + 1].at - transitions[i].at, EXPECTED_MS[i], 19) << "phase " << i;
  }

  // Releasing stops both timers
  sensor.publish_state(false);
  const size_t count = transitions.size();
  run_scheduler_for(250);
  EXPECT_EQ(transitions.size(), count);
  EXPECT_FALSE(sensor.state);
}

}  // namespace esphome::binary_sensor::testing
