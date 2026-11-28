#include <gtest/gtest.h>

#include "esphome/components/template/datetime/template_datetime.h"
#include "../record_action.h"

namespace esphome::template_::testing {

struct DateTimeState {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  bool operator==(const DateTimeState &) const = default;
};

static DateTimeState state_of(const TemplateDateTime &entity) {
  return {entity.year, entity.month, entity.day, entity.hour, entity.minute, entity.second};
}

class TemplateDateTimeControl : public ::testing::Test {
 protected:
  void SetUp() override {
    this->automation_.add_action(&this->record_);
    this->entity_.get_set_trigger()->set_automation_parent(&this->automation_);
    this->entity_.set_optimistic(true);
    this->entity_.make_call().set_datetime(2026, 11, 28, 10, 20, 30).perform();
  }
  TemplateDateTime entity_;
  Automation<ESPTime> automation_;
  RecordAction<ESPTime> record_;
};

TEST_F(TemplateDateTimeControl, FullCallSetsEveryField) {
  EXPECT_EQ(state_of(this->entity_), (DateTimeState{2026, 11, 28, 10, 20, 30}));
  const auto &seen = this->record_.seen;
  EXPECT_EQ((DateTimeState{seen.year, seen.month, seen.day_of_month, seen.hour, seen.minute, seen.second}),
            (DateTimeState{2026, 11, 28, 10, 20, 30}));
}

// The trigger gets 0 for a field the call leaves out; the state keeps its current value
TEST_F(TemplateDateTimeControl, PartialCallKeepsTheOtherFields) {
  this->entity_.make_call().set_hour(5).set_second(0).perform();
  EXPECT_EQ(state_of(this->entity_), (DateTimeState{2026, 11, 28, 5, 20, 0}));
  const auto &seen = this->record_.seen;
  EXPECT_EQ((DateTimeState{seen.year, seen.month, seen.day_of_month, seen.hour, seen.minute, seen.second}),
            (DateTimeState{0, 0, 0, 5, 0, 0}));
}

TEST_F(TemplateDateTimeControl, NotOptimisticLeavesTheState) {
  this->entity_.set_optimistic(false);
  this->entity_.make_call().set_datetime(2030, 1, 2, 3, 4, 5).perform();
  EXPECT_EQ(state_of(this->entity_), (DateTimeState{2026, 11, 28, 10, 20, 30}));
  EXPECT_EQ(this->record_.seen.year, 2030);
}

}  // namespace esphome::template_::testing
