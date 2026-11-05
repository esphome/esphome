#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

#include "esphome/core/automation.h"

namespace esphome::testing {

using Seen = std::vector<std::tuple<char, std::string, int>>;

class ListRecordAction : public Action<std::string, int> {
 public:
  ListRecordAction(char label, Seen *seen) : label_(label), seen_(seen) {}
  void play(const std::string &value, const int &index) override {
    this->seen_->emplace_back(this->label_, value, index);
  }

 protected:
  char label_;
  Seen *seen_;
};

TEST(AutomationListTest, TriggersEachAutomationInOrder) {
  Seen seen;
  ListRecordAction first_action('1', &seen);
  ListRecordAction second_action('2', &seen);
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
  EXPECT_EQ(seen, (Seen{{'1', "tag", 7}, {'2', "tag", 7}}));
}

TEST(AutomationListTest, ListsOfDifferentSizesTriggerInOrder) {
  Seen seen;
  ListRecordAction action('a', &seen);
  Automation<std::string, int> automation;
  automation.add_action(&action);

  AutomationList<1, std::string, int> small;
  AutomationList<3, std::string, int> large;
  small.add(&automation);
  large.add(&automation);
  large.add(&automation);
  small.trigger("a", 1);
  large.trigger("b", 2);
  EXPECT_EQ(seen, (Seen{{'a', "a", 1}, {'a', "b", 2}, {'a', "b", 2}}));
}

}  // namespace esphome::testing
