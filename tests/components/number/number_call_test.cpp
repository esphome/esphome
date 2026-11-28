#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "esphome/components/number/number.h"

namespace esphome::number::testing {

static std::vector<float> controlled;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void record(Number * /*entity*/, float value) { controlled.push_back(value); }

class NumberCallTest : public ::testing::Test {
 protected:
  void SetUp() override {
    controlled.clear();
    this->number_.traits.set_min_value(0);
    this->number_.traits.set_max_value(10);
    this->number_.traits.set_step(1);
  }
  ApplyNumber<record> number_;
};

TEST_F(NumberCallTest, SetValueReachesControl) {
  this->number_.make_call().set_value(4).perform();
  EXPECT_EQ(controlled, (std::vector<float>{4}));
}

// An unset value (NAN internally) and an explicit NAN are both rejected without calling control()
TEST_F(NumberCallTest, MissingOrNanValueIsRejected) {
  this->number_.make_call().with_operation(NUMBER_OP_SET).perform();
  this->number_.make_call().set_value(NAN).perform();
  EXPECT_TRUE(controlled.empty());
}

TEST_F(NumberCallTest, IncrementIgnoresUnsetValue) {
  this->number_.publish_state(3);
  this->number_.make_call().number_increment(false).perform();
  EXPECT_EQ(controlled, (std::vector<float>{4}));
}

}  // namespace esphome::number::testing
