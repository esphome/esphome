#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "esphome/components/cover/automation.h"
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

// Publishes a new operation at the same position, as cover.template.publish in an on_opened action might.
class PublishOperationAction final : public Action<> {
 public:
  PublishOperationAction(TestCover *cover, CoverOperation operation) : cover_(cover), operation_(operation) {}
  void play() override { this->cover_->publish(this->cover_->position, this->operation_); }

 protected:
  TestCover *cover_;
  CoverOperation operation_;
};

struct Fired {
  int opened{0};
  int closed{0};
  int opening{0};
  int closing{0};
  int idle{0};
};

using InState = bool (*)(const Cover &);
static bool is_opened(const Cover &cover) { return cover.position == COVER_OPEN; }
static bool is_closed(const Cover &cover) { return cover.position == COVER_CLOSED; }
template<CoverOperation OP> static bool is_in(const Cover &cover) { return cover.current_operation == OP; }

// The check codegen emits for each on_opened, on_closed, on_opening, on_closing and on_idle automation.
// Codegen keeps the flag in a static; one context per test automation keeps the callback capture inline.
struct StateAutomation {
  Cover *cover;
  Automation<> automation;
  bool seen{false};
};

template<InState T> static void attach(Cover *cover, Action<> *action) {
  auto *ctx = new StateAutomation{cover, {}, false};  // NOLINT(cppcoreguidelines-owning-memory)
  ctx->automation.add_action(action);
  cover->add_on_state_callback([ctx]() {
    if (state_entered(ctx->seen, T(*ctx->cover)))
      ctx->automation.trigger();
  });
}

template<InState T> static void count(Cover *cover, int *counter) {
  attach<T>(cover, new CountAction(counter));  // NOLINT(cppcoreguidelines-owning-memory)
}

class CoverTriggersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    count<is_opened>(&this->cover_, &this->fired_.opened);
    count<is_closed>(&this->cover_, &this->fired_.closed);
    count<is_in<COVER_OPERATION_OPENING>>(&this->cover_, &this->fired_.opening);
    count<is_in<COVER_OPERATION_CLOSING>>(&this->cover_, &this->fired_.closing);
    count<is_in<COVER_OPERATION_IDLE>>(&this->cover_, &this->fired_.idle);
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
  attach<is_opened>(&this->cover_,
                    new RepublishAction(&this->cover_));  // NOLINT(cppcoreguidelines-owning-memory)
  this->cover_.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(this->fired_.opened, 1);
  EXPECT_EQ(this->fired_.idle, 1);
  EXPECT_EQ(this->cover_.tilt, 0.5f);
}

// An operation republished from its own trigger fires that trigger once.
TEST_F(CoverTriggersTest, OperationPublishFromTriggerDoesNotRefire) {
  attach<is_in<COVER_OPERATION_OPENING>>(
      &this->cover_,
      new RepublishAction(&this->cover_));  // NOLINT(cppcoreguidelines-owning-memory)
  this->cover_.publish(0.5f, COVER_OPERATION_OPENING);
  EXPECT_EQ(this->fired_.opening, 1);
  EXPECT_EQ(this->cover_.tilt, 0.5f);
}

class RecordAction final : public Action<> {
 public:
  RecordAction(std::string *log, char name) : log_(log), name_(name) {}
  void play() override { this->log_->push_back(this->name_); }

 protected:
  std::string *log_;
  char name_;
};

// Codegen checks the triggers in this order: on_opened, on_closed, on_closing, on_opening, on_idle.
TEST(CoverTriggerOrderTest, FiresInRegistrationOrder) {
  TestCover cover;
  std::string log;
  attach<is_opened>(&cover, new RecordAction(&log, 'o'));  // NOLINT(cppcoreguidelines-owning-memory)
  attach<is_closed>(&cover, new RecordAction(&log, 'c'));  // NOLINT(cppcoreguidelines-owning-memory)
  attach<is_in<COVER_OPERATION_CLOSING>>(&cover,
                                         new RecordAction(&log, 'C'));  // NOLINT(cppcoreguidelines-owning-memory)
  attach<is_in<COVER_OPERATION_OPENING>>(&cover,
                                         new RecordAction(&log, 'O'));       // NOLINT(cppcoreguidelines-owning-memory)
  attach<is_in<COVER_OPERATION_IDLE>>(&cover, new RecordAction(&log, 'i'));  // NOLINT(cppcoreguidelines-owning-memory)
  cover.publish(COVER_OPEN, COVER_OPERATION_OPENING);
  cover.publish(COVER_CLOSED, COVER_OPERATION_IDLE);
  cover.publish(0.5f, COVER_OPERATION_CLOSING);
  EXPECT_EQ(log, "oOciC");
}

// A publish from an action runs the triggers not yet run, then later callbacks such as a copy cover's.
TEST(CoverTriggerOrderTest, PublishFromTriggerRunsPendingTriggersFirst) {
  TestCover cover;
  std::string log;
  attach<is_opened>(&cover, new RecordAction(&log, 'o'));                    // NOLINT(cppcoreguidelines-owning-memory)
  attach<is_opened>(&cover, new RepublishAction(&cover));                    // NOLINT(cppcoreguidelines-owning-memory)
  attach<is_in<COVER_OPERATION_IDLE>>(&cover, new RecordAction(&log, 'i'));  // NOLINT(cppcoreguidelines-owning-memory)
  cover.add_on_state_callback([&log]() { log.push_back('x'); });
  cover.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(log, "oixx");
}

// Each trigger reads the live operation, so on_idle never sees the IDLE that an on_opened action replaced.
TEST(CoverNestedPublishTest, OperationPublishedFromTriggerReplacesOuterOperation) {
  TestCover cover;
  Fired fired;
  attach<is_opened>(
      &cover, new PublishOperationAction(&cover, COVER_OPERATION_CLOSING));  // NOLINT(cppcoreguidelines-owning-memory)
  count<is_in<COVER_OPERATION_CLOSING>>(&cover, &fired.closing);
  count<is_in<COVER_OPERATION_IDLE>>(&cover, &fired.idle);
  cover.publish(COVER_OPEN, COVER_OPERATION_IDLE);
  EXPECT_EQ(fired.closing, 1);
  EXPECT_EQ(fired.idle, 0);
  EXPECT_EQ(cover.current_operation, COVER_OPERATION_CLOSING);
}

}  // namespace esphome::cover::testing
