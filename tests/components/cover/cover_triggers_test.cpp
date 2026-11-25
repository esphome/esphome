#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/cover/cover.h"
#include "esphome/core/automation.h"

namespace esphome::cover::testing {

class TestCover final : public Cover {
 public:
  CoverTraits get_traits() override {
    CoverTraits traits;
    traits.set_supports_position(true);
    traits.set_supports_tilt(true);
    return traits;
  }
  void publish(float position, CoverOperation operation, float tilt = COVER_OPEN) {
    this->position = position;
    this->current_operation = operation;
    this->tilt = tilt;
    this->publish_state(false);
  }

 protected:
  void control(const CoverCall & /*call*/) override {}
};

class CountAction final : public Action<> {
 public:
  explicit CountAction(int *count) : count_(count) {}
  void play() override { ++*this->count_; }

 protected:
  int *count_;
};

// Publishes the same position again with a new tilt, as an on_opened action might.
class RepublishAction final : public Action<> {
 public:
  explicit RepublishAction(TestCover *cover) : cover_(cover) {}
  void play() override { this->cover_->publish(this->cover_->position, this->cover_->current_operation, 0.5f); }

 protected:
  TestCover *cover_;
};

struct Fired {
  int opened{0};
  int closed{0};
  int opening{0};
  int closing{0};
  int idle{0};
};

// What codegen registers for on_opened, on_closed, on_opening, on_closing and on_idle.
template<uint8_t ENTERED> static void attach(Cover *cover, Action<> *action) {
  auto *automation = new Automation<>();  // NOLINT(cppcoreguidelines-owning-memory)
  automation->add_action(action);
  cover->add_on_state_entered_callback([automation](uint8_t entered) {
    if (entered & ENTERED)
      automation->trigger();
  });
}

template<uint8_t ENTERED> static void count(Cover *cover, int *counter) {
  attach<ENTERED>(cover, new CountAction(counter));  // NOLINT(cppcoreguidelines-owning-memory)
}

class CoverTriggersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    count<COVER_ENTERED_OPEN>(&this->cover_, &this->fired_.opened);
    count<COVER_ENTERED_CLOSED>(&this->cover_, &this->fired_.closed);
    count<COVER_ENTERED_OPENING>(&this->cover_, &this->fired_.opening);
    count<COVER_ENTERED_CLOSING>(&this->cover_, &this->fired_.closing);
    count<COVER_ENTERED_IDLE>(&this->cover_, &this->fired_.idle);
  }

  TestCover cover_;
  Fired fired_;
};

TEST_F(CoverTriggersTest, FirstPublishOpenFiresOpenedAndIdle) {
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 1);
  EXPECT_EQ(this->fired_.closed, 0);
  EXPECT_EQ(this->fired_.idle, 1);
  EXPECT_EQ(this->fired_.opening, 0);
  EXPECT_EQ(this->fired_.closing, 0);
}

TEST_F(CoverTriggersTest, FirstPublishClosedFiresClosed) {
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 0);
  EXPECT_EQ(this->fired_.closed, 1);
  EXPECT_EQ(this->fired_.idle, 1);
}

TEST_F(CoverTriggersTest, FirstPublishBetweenFiresOnlyOperation) {
  this->cover_.publish(0.5f, COVER_OPERATION_OPENING);
  EXPECT_EQ(this->fired_.opened, 0);
  EXPECT_EQ(this->fired_.closed, 0);
  EXPECT_EQ(this->fired_.opening, 1);
  EXPECT_EQ(this->fired_.idle, 0);
}

TEST_F(CoverTriggersTest, RepeatedPublishDoesNotRefire) {
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 1);
  EXPECT_EQ(this->fired_.idle, 1);

  this->cover_.publish(0.4f, COVER_OPERATION_CLOSING);
  this->cover_.publish(0.3f, COVER_OPERATION_CLOSING);
  this->cover_.publish(0.2f, COVER_OPERATION_CLOSING);
  EXPECT_EQ(this->fired_.closing, 1);
}

TEST_F(CoverTriggersTest, TiltOnlyChangesDoNotFire) {
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE, 0.0f);
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE, 0.5f);
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE, 1.0f);
  EXPECT_EQ(this->fired_.closed, 1);
  EXPECT_EQ(this->fired_.opened, 0);
  EXPECT_EQ(this->fired_.idle, 1);
}

TEST_F(CoverTriggersTest, FullCycleFiresEachEdgeOnce) {
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE);
  this->cover_.publish(0.1f, COVER_OPERATION_OPENING);
  this->cover_.publish(0.6f, COVER_OPERATION_OPENING);
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  this->cover_.publish(0.7f, COVER_OPERATION_CLOSING);
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.closed, 2);
  EXPECT_EQ(this->fired_.opened, 1);
  EXPECT_EQ(this->fired_.opening, 1);
  EXPECT_EQ(this->fired_.closing, 1);
  EXPECT_EQ(this->fired_.idle, 3);
}

TEST_F(CoverTriggersTest, ReachingEndWhileStillMovingFiresPosition) {
  this->cover_.publish(0.5f, COVER_OPERATION_OPENING);
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_OPENING);
  EXPECT_EQ(this->fired_.opened, 1);
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 1);
  EXPECT_EQ(this->fired_.idle, 1);
  EXPECT_EQ(this->fired_.opening, 1);
}

TEST_F(CoverTriggersTest, OperationChangeWithoutPositionChange) {
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE);
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_OPENING);
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE);
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_OPENING);
  EXPECT_EQ(this->fired_.closed, 1);
  EXPECT_EQ(this->fired_.opening, 2);
  EXPECT_EQ(this->fired_.idle, 2);
}

TEST_F(CoverTriggersTest, DirectOpenToClosedFiresClosed) {
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  this->cover_.publish(COVER_CLOSED, COVER_OPERATION_IDLE);
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 2);
  EXPECT_EQ(this->fired_.closed, 1);
  EXPECT_EQ(this->fired_.idle, 1);
}

TEST_F(CoverTriggersTest, OutOfRangePositionIsClampedToEnd) {
  this->cover_.publish(0.5f, COVER_OPERATION_IDLE);
  this->cover_.publish(1.5f, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 1);
  this->cover_.publish(-0.5f, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.closed, 1);
}

TEST_F(CoverTriggersTest, UnknownPositionDoesNotFire) {
  this->cover_.publish(NAN, COVER_OPERATION_IDLE);
  this->cover_.publish(NAN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 0);
  EXPECT_EQ(this->fired_.closed, 0);
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 1);
}

TEST_F(CoverTriggersTest, PublishFromTriggerDoesNotRefire) {
  attach<COVER_ENTERED_OPEN>(&this->cover_,
                             new RepublishAction(&this->cover_));  // NOLINT(cppcoreguidelines-owning-memory)
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 1);
  EXPECT_EQ(this->fired_.idle, 1);
  EXPECT_EQ(this->cover_.tilt, 0.5f);
}

TEST(CoverStateCallbackTest, PlainCallbacksRunOnEveryPublish) {
  TestCover cover;
  int calls = 0;
  int mutable_calls = 0;
  cover.add_on_state_callback([&calls]() { ++calls; });
  cover.add_on_state_callback([&mutable_calls, seen = 0]() mutable { mutable_calls = ++seen; });
  cover.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  cover.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(mutable_calls, 2);
}

}  // namespace esphome::cover::testing
