#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/select/select.h"

namespace esphome::select::testing {

static constexpr const char *const CALL_OPTIONS[] = {"low", "medium", "high"};

static std::vector<size_t> controlled;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void record(Select * /*entity*/, size_t index) { controlled.push_back(index); }

class SelectCallTest : public ::testing::Test {
 protected:
  void SetUp() override {
    controlled.clear();
    this->select_.traits.set_options_static(CALL_OPTIONS, 3);
  }
  ApplySelect<record> select_;
};

TEST_F(SelectCallTest, SetIndexAndOptionReachControl) {
  this->select_.make_call().set_index(0).perform();  // 0 is an index, not "unset"
  this->select_.make_call().set_option("high").perform();
  EXPECT_EQ(controlled, (std::vector<size_t>{0, 2}));
}

TEST_F(SelectCallTest, UnsetUnknownOrOutOfRangeIsRejected) {
  this->select_.make_call().with_operation(SELECT_OP_SET).perform();
  this->select_.make_call().set_option("missing").perform();
  this->select_.make_call().set_index(3).perform();
  EXPECT_TRUE(controlled.empty());
}

TEST_F(SelectCallTest, OperationsWithoutAnIndex) {
  this->select_.make_call().select_last().perform();
  this->select_.make_call().select_first().perform();
  EXPECT_EQ(controlled, (std::vector<size_t>{2, 0}));
}

}  // namespace esphome::select::testing
