#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/number/number.h"

namespace esphome::number::testing {

static Number *last_entity = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<float> values;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void on_control(Number *entity, float value) {
  last_entity = entity;
  values.push_back(value);
}

TEST(ApplyNumber, ControlRunsTheFunctionWithTheValue) {
  last_entity = nullptr;
  values.clear();
  ApplyNumber<on_control> number;
  number.traits.set_min_value(0);
  number.traits.set_max_value(10);
  number.traits.set_step(1);
  number.make_call().set_value(4).perform();
  EXPECT_EQ(values, (std::vector<float>{4}));
  EXPECT_EQ(last_entity, &number);
}

TEST(ApplyNumber, HoldsNoMoreThanTheBase) { EXPECT_EQ(sizeof(ApplyNumber<on_control>), sizeof(Number)); }

}  // namespace esphome::number::testing
