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
    this->automation.add_action(&this->record);
    this->entity.get_set_trigger()->set_automation_parent(&this->automation);
    this->entity.set_optimistic(true);
    this->entity.make_call().set_datetime(2026, 11, 28, 10, 20, 30).perform();
  }
  TemplateDateTime entity;
  Automation<ESPTime> automation;
  RecordAction<ESPTime> record;
};

TEST_F(TemplateDateTimeControl, FullCallSetsEveryField) {
  EXPECT_EQ(state_of(this->entity), (DateTimeState{2026, 11, 28, 10, 20, 30}));
  const auto &seen = this->record.seen;
  EXPECT_EQ((DateTimeState{seen.year, seen.month, seen.day_of_month, seen.hour, seen.minute, seen.second}),
            (DateTimeState{2026, 11, 28, 10, 20, 30}));
}

// The trigger gets 0 for a field the call leaves out; the state keeps its current value
TEST_F(TemplateDateTimeControl, PartialCallKeepsTheOtherFields) {
  this->entity.make_call().set_hour(5).set_second(0).perform();
  EXPECT_EQ(state_of(this->entity), (DateTimeState{2026, 11, 28, 5, 20, 0}));
  const auto &seen = this->record.seen;
  EXPECT_EQ((DateTimeState{seen.year, seen.month, seen.day_of_month, seen.hour, seen.minute, seen.second}),
            (DateTimeState{0, 0, 0, 5, 0, 0}));
}

TEST_F(TemplateDateTimeControl, NotOptimisticLeavesTheState) {
  this->entity.set_optimistic(false);
  this->entity.make_call().set_datetime(2030, 1, 2, 3, 4, 5).perform();
  EXPECT_EQ(state_of(this->entity), (DateTimeState{2026, 11, 28, 10, 20, 30}));
  EXPECT_EQ(this->record.seen.year, 2030);
}

}  // namespace esphome::template_::testing
