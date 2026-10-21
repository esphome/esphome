#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome::output::testing {

static void *last_output = nullptr;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<bool> bool_writes;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<float> levels;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_binary_write(BinaryOutput *output, bool state) {
  last_output = output;
  bool_writes.push_back(state);
}

static void on_float_write(FloatOutput *output, float state) {
  last_output = output;
  levels.push_back(state);
}

TEST(ApplyBinaryOutput, WriteRunsTheFunctionWithTheState) {
  last_output = nullptr;
  bool_writes.clear();
  ApplyBinaryOutput<on_binary_write> out;
  out.turn_on();
  out.turn_off();
  EXPECT_EQ(bool_writes, (std::vector<bool>{true, false}));
  EXPECT_EQ(last_output, &out);
}

TEST(ApplyBinaryOutput, InversionIsAppliedBeforeTheFunction) {
  bool_writes.clear();
  ApplyBinaryOutput<on_binary_write> out;
  out.set_inverted(true);
  out.turn_on();
  EXPECT_EQ(bool_writes, (std::vector<bool>{false}));
}

TEST(ApplyFloatOutput, WriteRunsTheFunctionWithTheLevel) {
  last_output = nullptr;
  levels.clear();
  ApplyFloatOutput<on_float_write> out;
  out.set_level(0.25f);
  out.turn_on();
  out.turn_off();
  EXPECT_EQ(levels, (std::vector<float>{0.25f, 1.0f, 0.0f}));
  EXPECT_EQ(last_output, &out);
}

TEST(ApplyFloatOutput, InversionAndClampAreAppliedBeforeTheFunction) {
  levels.clear();
  ApplyFloatOutput<on_float_write> out;
  out.set_inverted(true);
  out.set_level(0.25f);
  out.set_level(2.0f);
  EXPECT_EQ(levels, (std::vector<float>{0.75f, 0.0f}));
}

TEST(ApplyOutput, HoldsNoMoreThanTheBase) {
  EXPECT_EQ(sizeof(ApplyBinaryOutput<on_binary_write>), sizeof(BinaryOutput));
  EXPECT_EQ(sizeof(ApplyFloatOutput<on_float_write>), sizeof(FloatOutput));
}

}  // namespace esphome::output::testing
