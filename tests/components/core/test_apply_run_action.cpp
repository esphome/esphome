#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

namespace esphome::core::testing {

namespace {

std::vector<int> run_log;
ActionList<> *list_to_stop{nullptr};
int running_seen{-1};

// Generated functions have this shape: one block per action, with a check between them
void merged_run(const int &esphome__running) {
  running_seen = esphome__running;
  run_log.push_back(1);
  if (list_to_stop != nullptr)
    list_to_stop->stop();
  if (!esphome__running)
    return;
  run_log.push_back(2);
}

int number_seen{0};
std::string text_seen;

void merged_run_with_args(const int &esphome__running, const int &number, const std::string &text) {
  number_seen = number;
  text_seen = text;
}

class RecordAction : public Action<> {
 public:
  void play() override { run_log.push_back(3); }
};

class ApplyRunActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    run_log.clear();
    list_to_stop = nullptr;
    running_seen = -1;
    this->list_.add_action(&this->run_);
    this->list_.add_action(&this->next_);
  }

  ApplyRunAction<merged_run> run_;
  RecordAction next_;
  ActionList<> list_;
};

}  // namespace

TEST_F(ApplyRunActionTest, RunsEveryActionThenContinuesTheChain) {
  this->list_.play();

  EXPECT_EQ(run_log, (std::vector<int>{1, 2, 3}));
  EXPECT_FALSE(this->list_.is_running());
}

TEST_F(ApplyRunActionTest, PassesTheRunCountWhileRunning) {
  this->list_.play();

  EXPECT_EQ(running_seen, 1);
}

TEST_F(ApplyRunActionTest, StopSkipsTheRestOfTheRunAndTheChain) {
  list_to_stop = &this->list_;

  this->list_.play();

  EXPECT_EQ(run_log, (std::vector<int>{1}));
  EXPECT_FALSE(this->list_.is_running());
}

TEST(ApplyRunAction, PassesTriggerArguments) {
  ApplyRunAction<merged_run_with_args, int, std::string> action;
  ActionList<int, std::string> list;
  list.add_action(&action);

  list.play(5, "x");

  EXPECT_EQ(number_seen, 5);
  EXPECT_EQ(text_seen, "x");
}

}  // namespace esphome::core::testing
