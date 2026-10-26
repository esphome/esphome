#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome::output::testing {

static void *last_output = nullptr;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint16_t last_index = 0;        // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<bool> bool_writes;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<float> levels;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_binary_write(BinaryOutput *output, uint16_t index, bool state) {
  last_output = output;
  last_index = index;
  bool_writes.push_back(state);
}

static void on_float_write(FloatOutput *output, uint16_t index, float state) {
  last_output = output;
  last_index = index;
  levels.push_back(state);
}

// The index sits in the base's tail padding; only the power supply requester leaves none.
#ifndef USE_POWER_SUPPLY
static_assert(sizeof(ApplyBinaryOutput<on_binary_write>) == sizeof(BinaryOutput));
static_assert(sizeof(ApplyFloatOutput<on_float_write>) == sizeof(FloatOutput));
#endif

static void reset() {
  last_output = nullptr;
  last_index = 0;
  bool_writes.clear();
  levels.clear();
}

TEST(ApplyBinaryOutput, WritesReachTheFunctionWithIndexAndInversion) {
  reset();
  ApplyBinaryOutput<on_binary_write> out(7);
  out.turn_on();
  out.turn_off();
  out.set_inverted(true);
  out.turn_on();
  EXPECT_EQ(bool_writes, (std::vector<bool>{true, false, false}));
  EXPECT_EQ(last_output, &out);
  EXPECT_EQ(last_index, 7);
}

TEST(ApplyFloatOutput, WritesReachTheFunctionWithIndexInversionAndClamp) {
  reset();
  ApplyFloatOutput<on_float_write> out(3);
  out.set_level(0.25f);
  out.turn_on();
  out.set_inverted(true);
  out.set_level(0.25f);
  out.set_level(2.0f);
  EXPECT_EQ(levels, (std::vector<float>{0.25f, 1.0f, 0.75f, 0.0f}));
  EXPECT_EQ(last_output, &out);
  EXPECT_EQ(last_index, 3);
}

#ifdef USE_OUTPUT_FLOAT_POWER_SCALING
TEST(ApplyFloatOutput, PowerScalingIsAppliedBeforeTheFunction) {
  reset();
  ApplyFloatOutput<on_float_write> out;
  out.set_min_power(0.2f);
  out.set_max_power(0.8f);
  out.set_zero_means_zero(true);
  out.set_level(0.5f);
  out.turn_off();
  ASSERT_EQ(levels.size(), 2u);
  EXPECT_FLOAT_EQ(levels[0], 0.5f);
  EXPECT_EQ(levels[1], 0.0f);
  EXPECT_EQ(last_index, 0);
}
#endif

}  // namespace esphome::output::testing
