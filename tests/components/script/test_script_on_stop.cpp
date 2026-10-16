#include <gtest/gtest.h>

#include "esphome/components/script/script.h"

namespace esphome::script::testing {

class CountAction : public Action<> {
 public:
  explicit CountAction(int *count) : count_(count) {}

 protected:
  void play() override { (*this->count_)++; }
  int *count_;
};

// Stays running until release() is called, like a delay that never expires on its own.
class HoldAction : public Action<> {
 public:
  void play_complex() override {
    this->num_running_++;
    this->plays++;
  }
  void release() { this->play_next_(); }
  int plays{0};

 protected:
  void play() override {}
};

class QueueingScriptOnStopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->script_.set_max_runs(3);
    this->automation_.add_action(&this->hold_);
    this->stop_automation_.add_action(&this->count_);
  }

  QueueingScript<> script_;
  Automation<> automation_{&this->script_};
  HoldAction hold_;
  int on_stop_calls_{0};
  Automation<> stop_automation_{this->script_.get_stop_trigger()};
  CountAction count_{&this->on_stop_calls_};
};

TEST_F(QueueingScriptOnStopTest, StopWhileRunningFiresOnStopOnceAndDropsQueuedRun) {
  this->script_.execute();
  this->script_.execute();
  ASSERT_EQ(this->hold_.plays, 1);

  this->script_.stop();
  EXPECT_EQ(this->on_stop_calls_, 1);

  this->script_.loop();
  EXPECT_EQ(this->hold_.plays, 1);
}

// After one run ends, the next queued run only starts on the following loop() call
TEST_F(QueueingScriptOnStopTest, StopBetweenQueuedRunsFiresOnStop) {
  this->script_.execute();
  this->script_.execute();
  this->hold_.release();
  ASSERT_FALSE(this->script_.is_running());

  this->script_.stop();
  EXPECT_EQ(this->on_stop_calls_, 1);

  this->script_.loop();
  EXPECT_EQ(this->hold_.plays, 1);
}

TEST_F(QueueingScriptOnStopTest, StopWhenIdleDoesNotFireOnStop) {
  this->script_.stop();
  EXPECT_EQ(this->on_stop_calls_, 0);
}

}  // namespace esphome::script::testing
