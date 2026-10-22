#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

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

class Counter {
 public:
  int value{0};
};

class IncrementAction : public Action<Counter &> {
 public:
  void play(Counter &counter) override { this->seen = ++counter.value; }
  int seen{0};
};

TEST(ScriptWaitActionTest, KeepsCopyOfMutableReferenceArguments) {
  FakeScript script;
  ScriptWaitAction<FakeScript, Counter &> wait(&script);
  IncrementAction increment;
  ActionList<Counter &> actions;
  actions.add_actions({&wait, &increment});

  Counter counter;
  actions.play(counter);
  script.running = false;
  wait.loop();
  EXPECT_EQ(increment.seen, 1);
  // Like delay, the actions after the wait work on a copy
  EXPECT_EQ(counter.value, 0);
}

class Entity {
 public:
  Entity() = default;
  Entity(const Entity &) = delete;
  int value{0};
};

class EntityIncrementAction : public Action<Entity &> {
 public:
  void play(Entity &entity) override { entity.value++; }
};

// A type that cannot be copied, such as an entity, is kept by reference
TEST(ScriptWaitActionTest, KeepsReferenceToNonCopyableArguments) {
  FakeScript script;
  ScriptWaitAction<FakeScript, Entity &> wait(&script);
  EntityIncrementAction increment;
  ActionList<Entity &> actions;
  actions.add_actions({&wait, &increment});

  Entity entity;
  actions.play(entity);
  script.running = false;
  wait.loop();
  EXPECT_EQ(entity.value, 1);
}

class StopWaitAction : public Action<const std::string &> {
 public:
  void play(const std::string &value) override {
    this->wait->stop_complex();
    this->seen = value;
  }
  Action<const std::string &> *wait{nullptr};
  std::string seen;
};

// The chain after the wait may stop it, e.g. a script with mode: restart
TEST(ScriptWaitActionTest, StopDuringChainKeepsArguments) {
  FakeScript script;
  ScriptWaitAction<FakeScript, const std::string &> wait(&script);
  StopWaitAction stop;
  stop.wait = &wait;
  ActionList<const std::string &> actions;
  actions.add_actions({&wait, &stop});

  // With a second item waiting, stop() clears the queue while the first item's chain runs
  actions.play(std::string("argument longer than the small string buffer"));
  actions.play(std::string("second argument, also longer than the buffer"));
  script.running = false;
  wait.loop();
  EXPECT_EQ(stop.seen, "argument longer than the small string buffer");
  wait.loop();
  EXPECT_EQ(stop.seen, "argument longer than the small string buffer");
}

class RestartAction : public Action<const std::string &> {
 public:
  void play(const std::string &value) override {
    this->seen.push_back(value);
    if (this->seen.size() > 1)
      return;
    // What a script with mode: restart does: stop, then run again while the awaited script is busy
    this->wait->stop_complex();
    this->script->running = true;
    this->actions->play(std::string("restarted, longer than the small string buffer"));
  }
  Action<const std::string &> *wait{nullptr};
  FakeScript *script{nullptr};
  ActionList<const std::string &> *actions{nullptr};
  std::vector<std::string> seen;
};

// The old pop_front() after the chain removed the item queued by the restart
TEST(ScriptWaitActionTest, RestartDuringChainKeepsNewItem) {
  FakeScript script;
  ScriptWaitAction<FakeScript, const std::string &> wait(&script);
  RestartAction restart;
  ActionList<const std::string &> actions;
  actions.add_actions({&wait, &restart});
  restart.wait = &wait;
  restart.script = &script;
  restart.actions = &actions;

  // With a second item waiting, the stop really clears the queue; the restart then queues a new item
  actions.play(std::string("first argument, longer than the small string buffer"));
  actions.play(std::string("second argument, dropped by the stop"));
  script.running = false;
  wait.loop();
  ASSERT_EQ(restart.seen.size(), 1u);

  script.running = false;
  wait.loop();
  ASSERT_EQ(restart.seen.size(), 2u);
  EXPECT_EQ(restart.seen[1], "restarted, longer than the small string buffer");
}

TEST(ScriptWaitActionTest, LoopDisabledOnceNothingWaits) {
  FakeScript script;
  ScriptWaitAction<FakeScript, const std::string &> wait(&script);
  RecordAction record;
  ActionList<const std::string &> actions;
  actions.add_actions({&wait, &record});

  actions.play(std::string("argument"));
  script.running = false;
  wait.loop();
  EXPECT_EQ(record.seen, "argument");
  wait.loop();
  EXPECT_TRUE(wait.is_idle());
}

}  // namespace esphome::script::testing
