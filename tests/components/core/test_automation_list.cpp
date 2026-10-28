#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/core/automation.h"

namespace esphome::testing {

class RecordStringAction : public Action<std::string, int> {
 public:
  explicit RecordStringAction(std::vector<std::string> *seen) : seen_(seen) {}
  void play(const std::string &value, const int &index) override {
    this->seen_->push_back(value + ":" + std::to_string(index));
  }

 protected:
  std::vector<std::string> *seen_;
};

TEST(AutomationListTest, TriggersEachAutomationInOrder) {
  std::vector<std::string> seen;
  RecordStringAction first_action(&seen);
  RecordStringAction second_action(&seen);
  Automation<std::string, int> first;
  Automation<std::string, int> second;
  first.add_action(&first_action);
  second.add_action(&second_action);

  AutomationList<2, std::string, int> list;
  EXPECT_TRUE(list.empty());
  list.trigger("none", 0);
  EXPECT_TRUE(seen.empty());

  list.add(&first);
  list.add(&second);
  EXPECT_FALSE(list.empty());
  list.trigger("tag", 7);
  EXPECT_EQ(seen, (std::vector<std::string>{"tag:7", "tag:7"}));
}

TEST(AutomationListTest, ListsOfDifferentSizesShareTriggerCode) {
  std::vector<std::string> seen;
  RecordStringAction action(&seen);
  Automation<std::string, int> automation;
  automation.add_action(&action);

  AutomationList<1, std::string, int> small;
  AutomationList<3, std::string, int> large;
  small.add(&automation);
  large.add(&automation);
  large.add(&automation);
  small.trigger("a", 1);
  large.trigger("b", 2);
  EXPECT_EQ(seen, (std::vector<std::string>{"a:1", "b:2", "b:2"}));
}

}  // namespace esphome::testing
