#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "esphome/core/base_automation.h"

namespace esphome::testing {

class FlagCondition : public Condition<const std::string &> {
 public:
  bool check(const std::string & /*value*/) override { return this->value; }
  bool value{true};
};

class RecordAction : public Action<const std::string &> {
 public:
  void play(const std::string &value) override { this->seen.push_back(value); }
  std::vector<std::string> seen;
};

// A body that finishes at once runs its next pass from loop(), on a copy of a trigger
// argument passed as `const T &`: the referenced object is gone once the trigger has returned
TEST(WhileActionTest, SynchronousBodyResumesFromLoopWithCopiedArguments) {
  FlagCondition condition;
  WhileAction<const std::string &> loop(&condition);
  RecordAction body, after;
  loop.add_then({&body});
  ActionList<const std::string &> actions;
  actions.add_actions({&loop, &after});

  {
    auto value = std::make_unique<std::string>("argument longer than the small string buffer");
    actions.play(*value);
  }
  EXPECT_EQ(body.seen.size(), 1u);
  EXPECT_TRUE(after.seen.empty());
  EXPECT_TRUE(actions.is_running());

  loop.loop();
  ASSERT_EQ(body.seen.size(), 2u);
  EXPECT_EQ(body.seen[1], "argument longer than the small string buffer");

  condition.value = false;
  loop.loop();
  EXPECT_EQ(body.seen.size(), 2u);
  ASSERT_EQ(after.seen.size(), 1u);
  EXPECT_EQ(after.seen[0], "argument longer than the small string buffer");
  EXPECT_FALSE(actions.is_running());
}

// Two runs of the same loop each keep their own pass pending
TEST(WhileActionTest, ParallelRunsEachResume) {
  FlagCondition condition;
  WhileAction<const std::string &> loop(&condition);
  RecordAction body, after;
  loop.add_then({&body});
  ActionList<const std::string &> actions;
  actions.add_actions({&loop, &after});

  actions.play("first");
  actions.play("second");
  EXPECT_EQ(body.seen, (std::vector<std::string>{"first", "second"}));

  condition.value = false;
  loop.loop();
  EXPECT_EQ(body.seen.size(), 2u);
  EXPECT_EQ(after.seen, (std::vector<std::string>{"first", "second"}));
  EXPECT_FALSE(actions.is_running());
}

}  // namespace esphome::testing
