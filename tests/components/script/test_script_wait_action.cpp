#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "esphome/components/script/script.h"

namespace esphome::script::testing {

class FakeScript {
 public:
  bool is_running() const { return this->running; }
  bool running{true};
};

class RecordAction : public Action<const std::string &> {
 public:
  void play(const std::string &value) override { this->seen = value; }
  std::string seen;
};

// A trigger argument passed as `const T &` must be copied while waiting: the
// referenced object is gone once the trigger has returned.
TEST(ScriptWaitActionTest, KeepsCopyOfReferenceArguments) {
  FakeScript script;
  ScriptWaitAction<FakeScript, const std::string &> wait(&script);
  RecordAction record;
  ActionList<const std::string &> actions;
  actions.add_actions({&wait, &record});

  {
    auto value = std::make_unique<std::string>("argument longer than the small string buffer");
    actions.play(*value);
  }
  EXPECT_TRUE(record.seen.empty());

  script.running = false;
  wait.loop();
  EXPECT_EQ(record.seen, "argument longer than the small string buffer");
}

}  // namespace esphome::script::testing
