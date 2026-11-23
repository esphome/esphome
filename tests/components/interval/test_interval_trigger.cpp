#include <gtest/gtest.h>

#include "esphome/components/interval/interval.h"
#include "esphome/core/application.h"

namespace esphome::interval::testing {

class CountAction : public Action<> {
 public:
  explicit CountAction(int *count) : count_(count) {}
  void play() override { (*this->count_)++; }

 protected:
  int *count_;
};

TEST(IntervalTrigger, UpdateRunsTheAutomationAtDataPriority) {
  IntervalTrigger trigger;
  Automation<> automation{&trigger};
  int count = 0;
  CountAction action{&count};
  automation.add_action(&action);

  EXPECT_EQ(trigger.get_setup_priority(), setup_priority::DATA);
  EXPECT_FALSE(HasLoopOverride<IntervalTrigger>::value);
  trigger.set_update_interval(1000);
  EXPECT_EQ(trigger.get_update_interval(), 1000u);

  trigger.update();
  trigger.update();
  EXPECT_EQ(count, 2);
}

}  // namespace esphome::interval::testing
