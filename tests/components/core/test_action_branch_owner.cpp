#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

namespace esphome::core::testing {

using Log = std::vector<int>;

// Records its id when it runs, then continues the chain
class RecordAction : public Action<> {
 public:
  RecordAction(Log *log, int id) : log_(log), id_(id) {}
  void play() override { this->log_->push_back(this->id_); }
  Action<> *next() const { return this->next_; }

 protected:
  Log *log_;
  int id_;
};

// Holds the chain until finish() is called, like a delay
class DeferredAction : public Action<> {
 public:
  explicit DeferredAction(Log *log, int id) : log_(log), id_(id) {}
  void play_complex() override {
    this->num_running_++;
    this->log_->push_back(this->id_);
    this->pending_ = true;
  }
  void play() override {}
  void stop() override { this->pending_ = false; }
  void finish() {
    if (this->pending_) {
      this->pending_ = false;
      this->play_next_();
    }
  }
  bool pending() const { return this->pending_; }

 protected:
  Log *log_;
  int id_;
  bool pending_{false};
};

class FlagCondition : public Condition<> {
 public:
  bool check() override { return this->value; }
  bool value{true};
};

// Stops the given list during check(), like script.stop in a lambda condition
class StoppingCondition : public Condition<> {
 public:
  bool check() override {
    if (this->list != nullptr)
      this->list->stop();
    return true;
  }
  ActionList<> *list{nullptr};
};

// Stops the given list on the Nth check and keeps returning true
class StopOnCheckCondition : public Condition<> {
 public:
  explicit StopOnCheckCondition(int stop_on) : stop_on_(stop_on) {}
  bool check() override {
    if (++this->checks_ == this->stop_on_ && this->list != nullptr)
      this->list->stop();
    return true;
  }
  ActionList<> *list{nullptr};

 protected:
  int stop_on_;
  int checks_{0};
};

// Turns its condition off after a number of checks
template<typename... Ts> class CountdownCondition : public Condition<Ts...> {
 public:
  explicit CountdownCondition(int passes) : passes_(passes) {}
  bool check(const Ts &.../*x*/) override { return this->passes_-- > 0; }

 protected:
  int passes_;
};

// Reads an action's next_ through Action's own tag helpers
class TaggedOwner : public RecordAction {
 public:
  using RecordAction::RecordAction;
  static bool is_tagged(const TaggedOwner *a) { return a->next_is_owner_(); }
  static Action<> *owner_of(const TaggedOwner *a) { return a->owner_(); }
};

class InspectList : public ActionList<> {
 public:
  Action<> *head() const { return this->actions_; }
};

TEST(ActionBranchOwner, SetOwnerTagsOnlyTheLastAction) {
  Log log;
  TaggedOwner a(&log, 1), b(&log, 2), c(&log, 3);
  TaggedOwner owner(&log, 99);
  InspectList list;
  list.add_actions({&a, &b});
  list.add_action(&c);  // appending before set_owner keeps order
  list.set_owner(&owner);
  EXPECT_EQ(list.head(), &a);
  EXPECT_EQ(a.next(), &b);
  EXPECT_EQ(b.next(), &c);
  EXPECT_FALSE(TaggedOwner::is_tagged(&a));
  EXPECT_FALSE(TaggedOwner::is_tagged(&b));
  EXPECT_TRUE(TaggedOwner::is_tagged(&c));
  EXPECT_EQ(TaggedOwner::owner_of(&c), &owner);
}

TEST(ActionBranchOwner, EmptyListStaysEmpty) {
  Log log;
  TaggedOwner owner(&log, 99);
  InspectList list;
  list.set_owner(&owner);
  EXPECT_EQ(list.head(), nullptr);
  EXPECT_TRUE(list.empty());
}

// ESPHOME_DEBUG_ASSERT is only live because script/cpp_unit_test.py builds with -DESPHOME_DEBUG
TEST(ActionBranchOwnerDeathTest, AppendAfterSetOwnerAsserts) {
  Log log;
  RecordAction a(&log, 1), late(&log, 2);
  TaggedOwner owner(&log, 99);
  ActionList<> list;
  list.add_action(&a);
  list.set_owner(&owner);
  EXPECT_DEATH(list.add_action(&late), "");
}

TEST(ActionBranchOwner, IfThenResumesAfterTheIfOnce) {
  Log log;
  FlagCondition cond;
  IfAction<true> if_action(&cond);
  RecordAction t1(&log, 1), t2(&log, 2), e1(&log, 3), after(&log, 4);
  if_action.add_then({&t1, &t2});
  if_action.add_else({&e1});
  ActionList<> top;
  top.add_actions({&if_action, &after});

  top.play();
  EXPECT_EQ(log, (Log{1, 2, 4}));
  log.clear();
  cond.value = false;
  top.play();
  EXPECT_EQ(log, (Log{3, 4}));
  EXPECT_FALSE(top.is_running());
  EXPECT_EQ(top.num_running(), 0);
}

// The per-item calls automation.py emits
TEST(ActionBranchOwner, PerItemIfAndWhileResumeAfterTheirBranch) {
  Log log;
  FlagCondition cond;
  IfAction<true> if_action(&cond);
  RecordAction t1(&log, 1), t2(&log, 2), e1(&log, 3), after(&log, 4);
  if_action.add_then_action(&t1);
  if_action.add_then_action(&t2);
  if_action.finish_then();
  if_action.add_else_action(&e1);
  if_action.finish_else();
  ActionList<> top;
  top.add_actions({&if_action, &after});
  top.play();
  cond.value = false;
  top.play();
  EXPECT_EQ(log, (Log{1, 2, 4, 3, 4}));

  log.clear();
  CountdownCondition<> loop_cond(2);
  WhileAction<> loop(&loop_cond);
  DeferredAction wait(&log, 5);
  RecordAction after2(&log, 6);
  loop.add_then_action(&wait);
  loop.finish_then();
  ActionList<> top2;
  top2.add_actions({&loop, &after2});
  top2.play();
  wait.finish();
  wait.finish();
  EXPECT_EQ(log, (Log{5, 5, 6}));
  EXPECT_FALSE(top2.is_running());
}

// then: [] generates finish_then() with no actions
TEST(ActionBranchOwner, PerItemEmptyBranchesFinishImmediately) {
  Log log;
  FlagCondition cond;
  IfAction<true> if_action(&cond);
  if_action.finish_then();
  if_action.finish_else();
  WhileAction<> loop(&cond);
  loop.finish_then();
  RecordAction after(&log, 1), after2(&log, 2);
  ActionList<> top, top2;
  top.add_actions({&if_action, &after});
  top2.add_actions({&loop, &after2});
  top.play();
  cond.value = false;
  top.play();
  cond.value = true;
  top2.play();
  EXPECT_EQ(log, (Log{1, 1, 2}));
  EXPECT_FALSE(top2.is_running());
}

TEST(ActionBranchOwner, ConditionThatStopsTheRunStartsNoBranch) {
  Log log;
  StoppingCondition cond;
  IfAction<true> if_action(&cond);
  WhileAction<> loop(&cond);
  RecordAction t1(&log, 1), e1(&log, 2), body(&log, 3), after(&log, 4);
  if_action.add_then({&t1});
  if_action.add_else({&e1});
  loop.add_then({&body});
  ActionList<> if_list, while_list;
  if_list.add_actions({&if_action, &after});
  while_list.add_actions({&loop, &after});

  cond.list = &if_list;
  if_list.play();
  cond.list = &while_list;
  while_list.play();
  EXPECT_TRUE(log.empty());
  EXPECT_FALSE(if_list.is_running());
  EXPECT_FALSE(while_list.is_running());
}

TEST(ActionBranchOwner, WhileConditionStoppingOnALaterPassEndsTheLoop) {
  Log log;
  StopOnCheckCondition cond(2);
  WhileAction<> loop(&cond);
  RecordAction body(&log, 1), after(&log, 2);
  loop.add_then({&body});
  ActionList<> list;
  list.add_actions({&loop, &after});
  cond.list = &list;

  list.play();
  EXPECT_EQ(log, (Log{1}));
  EXPECT_FALSE(list.is_running());
}

TEST(ActionBranchOwner, IfResumesAfterDeferredLastAction) {
  Log log;
  FlagCondition cond;
  IfAction<false> if_action(&cond);
  DeferredAction wait(&log, 1);
  RecordAction after(&log, 2);
  if_action.add_then({&wait});
  ActionList<> top;
  top.add_actions({&if_action, &after});

  top.play();
  EXPECT_EQ(log, (Log{1}));
  EXPECT_TRUE(top.is_running());
  // num_running() counts the top-level chain only: the if, not the action inside its branch
  EXPECT_EQ(top.num_running(), 1);
  wait.finish();
  EXPECT_EQ(log, (Log{1, 2}));
  EXPECT_FALSE(top.is_running());
  EXPECT_EQ(top.num_running(), 0);
}

// A deferred action that can hold two runs at once, completed in order
class QueueDeferredAction : public Action<> {
 public:
  void play_complex() override {
    this->num_running_++;
    this->pending_++;
  }
  void play() override {}
  void finish_one() {
    if (this->pending_ > 0) {
      this->pending_--;
      this->play_next_();
    }
  }
  int running() const { return this->num_running_; }

 protected:
  int pending_{0};
};

TEST(ActionBranchOwner, OverlappingRunsResumeOncePerRun) {
  Log log;
  FlagCondition cond;
  IfAction<false> if_action(&cond);
  QueueDeferredAction wait;
  RecordAction after(&log, 7);
  if_action.add_then({&wait});
  ActionList<> top;
  top.add_actions({&if_action, &after});

  top.play();
  top.play();
  EXPECT_EQ(wait.running(), 2);  // the tagged last action holds both runs
  wait.finish_one();
  EXPECT_EQ(log, (Log{7}));
  EXPECT_TRUE(top.is_running());
  wait.finish_one();
  EXPECT_EQ(log, (Log{7, 7}));
  EXPECT_FALSE(top.is_running());
  EXPECT_EQ(wait.running(), 0);
}

TEST(ActionBranchOwner, EmptyBranchesFinishImmediately) {
  Log log;
  FlagCondition cond;
  IfAction<true> if_action(&cond);
  RecordAction after(&log, 1);
  if_action.add_then({});
  if_action.add_else({});
  ActionList<> top;
  top.add_actions({&if_action, &after});
  top.play();
  cond.value = false;
  top.play();
  EXPECT_EQ(log, (Log{1, 1}));

  FlagCondition always;
  WhileAction<> loop(&always);
  loop.add_then({});
  ActionList<> top2;
  RecordAction after2(&log, 2);
  top2.add_actions({&loop, &after2});
  top2.play();
  EXPECT_EQ(log, (Log{1, 1, 2}));
  EXPECT_FALSE(top2.is_running());
}

TEST(ActionBranchOwner, WhileRechecksEachPassAndExits) {
  Log log;
  CountdownCondition<> cond(3);
  WhileAction<> loop(&cond);
  RecordAction body(&log, 1);
  RecordAction after(&log, 2);
  loop.add_then({&body});
  ActionList<> top;
  top.add_actions({&loop, &after});
  top.play();
  EXPECT_EQ(log, (Log{1, 1, 1, 2}));
  EXPECT_FALSE(top.is_running());
}

TEST(ActionBranchOwner, StopMidBranchClearsRunning) {
  Log log;
  FlagCondition cond;
  WhileAction<> loop(&cond);
  DeferredAction wait(&log, 1);
  RecordAction after(&log, 2);
  loop.add_then({&wait});
  ActionList<> top;
  top.add_actions({&loop, &after});

  top.play();
  EXPECT_TRUE(top.is_running());
  top.stop();
  EXPECT_FALSE(top.is_running());
  EXPECT_EQ(top.num_running(), 0);
  wait.finish();  // already stopped: nothing pending, nothing resumes
  EXPECT_EQ(log, (Log{1}));

  // A fresh run works after the stop
  cond.value = false;
  top.play();
  EXPECT_EQ(log, (Log{1, 2}));
}

TEST(ActionBranchOwner, NestedIfInsideWhileResumesEachOwner) {
  Log log;
  CountdownCondition<> loop_cond(2);
  FlagCondition if_cond;
  WhileAction<> loop(&loop_cond);
  IfAction<false> inner(&if_cond);
  DeferredAction wait(&log, 1);
  RecordAction after(&log, 9);
  inner.add_then({&wait});
  loop.add_then({&inner});  // the if is the last action of the loop body
  ActionList<> top;
  top.add_actions({&loop, &after});

  top.play();
  EXPECT_EQ(log, (Log{1}));
  wait.finish();  // resumes the if, then the while, which loops again
  EXPECT_EQ(log, (Log{1, 1}));
  wait.finish();  // second pass ends; the while condition is now false
  EXPECT_EQ(log, (Log{1, 1, 9}));
  EXPECT_FALSE(top.is_running());
  EXPECT_EQ(top.num_running(), 0);
}

// Records how deep the stack is on each pass. It works after the chain continues, as real actions do (a
// destructor, a log), so the compiler cannot turn the recursion into a jump.
class StackDepthAction : public Action<int> {
 public:
  void play_complex(const int &x) override {
    this->num_running_++;
    volatile char here = 0;
    auto at = reinterpret_cast<uintptr_t>(&here);
    this->lowest = std::min(this->lowest, at);
    this->highest = std::max(this->highest, at);
    this->play_next_(x);
    this->passes = this->passes + 1;
  }
  void play(const int & /*x*/) override {}
  uintptr_t lowest{UINTPTR_MAX};
  uintptr_t highest{0};
  volatile int passes{0};
};

// Each pass of a body that never waits starts at the same depth, not deeper than the last
TEST(ActionBranchOwner, WhileBodyThatNeverWaitsKeepsTheStackFlat) {
  CountdownCondition<int> cond(1000);
  WhileAction<int> loop(&cond);
  StackDepthAction body;
  loop.add_then({&body});
  ActionList<int> list;
  list.add_actions({&loop});

  list.play(1);
  EXPECT_EQ(body.passes, 1000);
  EXPECT_LT(body.highest - body.lowest, 256U);
  EXPECT_FALSE(list.is_running());
}

// Keeps the first argument by reference, like wait_until does for an entity, and copies the second
class MixedHandoffAction : public Action<const int &, int> {
 public:
  void play_complex(const int &entity, const int &value) override {
    this->num_running_++;
    this->finish();
    this->entity_ = &entity;
    this->value_ = value;
    this->waiting_ = true;
  }
  void play(const int & /*entity*/, const int & /*value*/) override {}
  void finish() {
    if (this->waiting_) {
      this->waiting_ = false;
      int released = this->value_;
      this->play_next_(*this->entity_, released);
    }
  }

 protected:
  const int *entity_{nullptr};
  int value_{0};
  bool waiting_{false};
};

class OncePerSecondValueCondition : public Condition<const int &, int> {
 public:
  bool check(const int & /*entity*/, const int &x) override { return !std::exchange(this->seen_[x], true); }

 protected:
  bool seen_[4]{};
};

class RecordSecondAction : public Action<const int &, int> {
 public:
  explicit RecordSecondAction(Log *log) : log_(log) {}
  void play(const int & /*entity*/, const int &x) override { this->log_->push_back(x); }

 protected:
  Log *log_;
};

// The end of another run's body inside this run's body must resume that run, not end this one. Runs on the same
// entity share its storage; the copied argument still tells them apart.
TEST(ActionBranchOwner, WhileTellsRunsApartBySharedAndCopiedArguments) {
  OncePerSecondValueCondition cond;
  WhileAction<const int &, int> loop(&cond);
  MixedHandoffAction wait;
  Log log;
  RecordSecondAction after(&log);
  loop.add_then({&wait});
  ActionList<const int &, int> list;
  list.add_actions({&loop, &after});

  const int entity = 0;
  list.play(entity, 1);
  list.play(entity, 2);
  EXPECT_EQ(log, (Log{1}));
  wait.finish();
  EXPECT_EQ(log, (Log{1, 2}));
  EXPECT_FALSE(list.is_running());
}

class CountPassAction : public Action<> {
 public:
  void play() override { this->passes++; }
  int passes{0};
};

// Without arguments a wait_until timeout of one run can end inside another run's pass. Runs without arguments
// are alike: whichever frame takes that end loops once for it, so every pass and every run still continues once.
TEST(ActionBranchOwner, WhileWithoutArgumentsKeepsRunsWhenAWaitTimesOutInside) {
  CountdownCondition<> cond(3);
  WhileAction<> loop(&cond);
  CountPassAction pass;
  FlagCondition never;
  never.value = false;
  WaitUntilAction<> wait(&never);
  wait.set_timeout_value([]() -> uint32_t { return 10; });
  Log log;
  RecordAction after(&log, 1);
  loop.add_then({&pass, &wait});
  ActionList<> list;
  list.add_actions({&loop, &after});

  list.play();  // run A waits
  delay(15);
  list.play();  // run B's pass releases A, whose wait timed out, inside it
  delay(15);
  wait.loop();  // the remaining waits time out
  EXPECT_EQ(pass.passes, 3);
  EXPECT_EQ(log, (Log{1, 1}));
  EXPECT_FALSE(list.is_running());
}

// Releases the run that waits here, then lets the current run go on at once: two ends in one pass
class ReleaseAndPassAction : public Action<> {
 public:
  void play_complex() override {
    this->num_running_++;
    if (this->waiting_) {
      this->waiting_ = false;
      this->play_next_();
    }
    if (this->pass) {
      this->play_next_();
      return;
    }
    this->waiting_ = true;
  }
  void play() override {}
  bool pass{false};

 protected:
  bool waiting_{false};
};

// Without arguments both ends of one pass mark the same body; the second must still continue its run
TEST(ActionBranchOwner, WhileWithoutArgumentsContinuesBothEndsOfOnePass) {
  CountdownCondition<> cond(2);
  WhileAction<> loop(&cond);
  ReleaseAndPassAction wait;
  Log log;
  RecordAction after(&log, 1);
  loop.add_then({&wait});
  ActionList<> list;
  list.add_actions({&loop, &after});

  list.play();  // run A waits
  wait.pass = true;
  list.play();  // run B's pass releases A and ends itself
  EXPECT_EQ(log, (Log{1, 1}));
  EXPECT_FALSE(list.is_running());
}

}  // namespace esphome::core::testing
