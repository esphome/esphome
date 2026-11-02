#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

namespace esphome::core::testing {

namespace {

// Trigger argument for the generated-style functions below, which are template arguments and cannot capture
struct RunState {
  std::vector<int> log{};
  ActionList<RunState *> *list_to_stop{nullptr};
  int running_seen{-1};
};

// Generated functions have this shape: one block per action, with a check between them
void merged_run(const int &running, RunState *const &state) {
  state->running_seen = running;
  state->log.push_back(1);
  if (state->list_to_stop != nullptr)
    state->list_to_stop->stop();
  if (!running)
    return;
  state->log.push_back(2);
}

void merged_run_with_args(const int &running, const int &number, std::string *const &text) {
  *text = std::string("got ") + (running != 0 && number == 5 ? "5 while running" : "something else");
}

class RecordAction : public Action<RunState *> {
 public:
  void play(RunState *const &state) override { state->log.push_back(3); }
};

class ApplyRunActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->list_.add_action(&this->run_);
    this->list_.add_action(&this->next_);
  }

  ApplyRunAction<merged_run, RunState *> run_;
  RecordAction next_;
  ActionList<RunState *> list_;
  RunState state_;
};

}  // namespace

TEST_F(ApplyRunActionTest, RunsEveryActionThenContinuesTheChain) {
  this->list_.play(&this->state_);

  EXPECT_EQ(this->state_.log, (std::vector<int>{1, 2, 3}));
  EXPECT_FALSE(this->list_.is_running());
}

TEST_F(ApplyRunActionTest, PassesTheRunCountWhileRunning) {
  this->list_.play(&this->state_);

  EXPECT_EQ(this->state_.running_seen, 1);
}

TEST_F(ApplyRunActionTest, StopSkipsTheRestOfTheRunAndTheChain) {
  this->state_.list_to_stop = &this->list_;

  this->list_.play(&this->state_);

  EXPECT_EQ(this->state_.log, (std::vector<int>{1}));
  EXPECT_FALSE(this->list_.is_running());
}

TEST(ApplyRunAction, PassesTriggerArguments) {
  ApplyRunAction<merged_run_with_args, int, std::string *> action;
  ActionList<int, std::string *> list;
  list.add_action(&action);
  std::string text;

  list.play(5, &text);

  EXPECT_EQ(text, "got 5 while running");
}

}  // namespace esphome::core::testing
