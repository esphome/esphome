#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/button/button.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/button/output_button.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::output::testing {

static std::vector<bool> writes;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_write(BinaryOutput * /*output*/, uint16_t /*index*/, bool state) { writes.push_back(state); }

static void on_press(button::Button * /*entity*/) {}

static void run_scheduler_after(uint32_t ms) {
  delay(ms);
  App.scheduler.call(millis());
}

TEST(OutputButton, TurnsOnAndOffAfterTheDuration) {
  writes.clear();
  ApplyBinaryOutput<on_write> out(0);
  button::ApplyButton<on_press> button;
  press_output_button(&button, &out, 50);
  EXPECT_EQ(writes, (std::vector<bool>{true}));
  run_scheduler_after(100);
  EXPECT_EQ(writes, (std::vector<bool>{true, false}));
}

TEST(OutputButton, SecondPressRestartsTheReset) {
  writes.clear();
  ApplyBinaryOutput<on_write> out(0);
  button::ApplyButton<on_press> button;
  press_output_button(&button, &out, 150);
  run_scheduler_after(100);
  press_output_button(&button, &out, 150);
  // Past the first press's reset, inside the second's
  run_scheduler_after(100);
  EXPECT_EQ(writes, (std::vector<bool>{true, true}));
  run_scheduler_after(100);
  EXPECT_EQ(writes, (std::vector<bool>{true, true, false}));
}

TEST(OutputButton, ButtonsResetIndependently) {
  writes.clear();
  ApplyBinaryOutput<on_write> out(0);
  button::ApplyButton<on_press> first;
  button::ApplyButton<on_press> second;
  press_output_button(&first, &out, 50);
  press_output_button(&second, &out, 50);
  run_scheduler_after(100);
  EXPECT_EQ(writes, (std::vector<bool>{true, true, false, false}));
}

}  // namespace esphome::output::testing
