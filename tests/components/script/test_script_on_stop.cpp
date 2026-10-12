#include <gtest/gtest.h>

#include "esphome/components/script/script.h"

namespace esphome::script::testing {

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
    this->script.set_max_runs(3);
    this->automation.add_action(&this->hold);
    this->script.add_on_stop_callback([this]() { this->on_stop_calls++; });
  }

  QueueingScript<> script;
  Automation<> automation{&this->script};
  HoldAction hold;
  int on_stop_calls{0};
};

TEST_F(QueueingScriptOnStopTest, StopWhileRunningFiresOnStopOnceAndDropsQueuedRun) {
  this->script.execute();
  this->script.execute();
  ASSERT_EQ(this->hold.plays, 1);

  this->script.stop();
  EXPECT_EQ(this->on_stop_calls, 1);

  this->script.loop();
  EXPECT_EQ(this->hold.plays, 1);
}

// After one run ends, the next queued run only starts on the following loop() call
TEST_F(QueueingScriptOnStopTest, StopBetweenQueuedRunsFiresOnStop) {
  this->script.execute();
  this->script.execute();
  this->hold.release();
  ASSERT_FALSE(this->script.is_running());

  this->script.stop();
  EXPECT_EQ(this->on_stop_calls, 1);

  this->script.loop();
  EXPECT_EQ(this->hold.plays, 1);
}

TEST_F(QueueingScriptOnStopTest, StopWhenIdleDoesNotFireOnStop) {
  this->script.stop();
  EXPECT_EQ(this->on_stop_calls, 0);
}

}  // namespace esphome::script::testing
