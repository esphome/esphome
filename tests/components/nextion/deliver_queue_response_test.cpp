#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <string>

// Expose Nextion's test-required members as public.
#define GTEST_TESTING
#include "esphome/components/nextion/nextion.h"
#include "esphome/components/nextion/nextion_component.h"

namespace esphome::nextion::testing {

// ---------------------------------------------------------------------------
// Mock component that records whether its state was set.
// ---------------------------------------------------------------------------
class MockSensorComponent : public NextionComponentBase {
 public:
  explicit MockSensorComponent(const std::string &name, NextionQueueType type) : name_(name), queue_type_(type) {
    this->set_variable_name(name);
  }

  NextionQueueType get_queue_type() const override { return queue_type_; }
  const char *get_queue_type_string() const override { return NEXTION_QUEUE_TYPE_STRINGS[this->get_queue_type()]; }

  void set_state_from_int(int state_value, bool publish, bool send_to_nextion) override {
    state_set_ = true;
    int_value_ = state_value;
  }
  void set_state_from_string(const std::string &state_value, bool publish, bool send_to_nextion) override {
    state_set_ = true;
    str_value_ = state_value;
  }

  bool state_set_ = false;
  int int_value_ = 0;
  std::string str_value_;
  const std::string name_;
  const NextionQueueType queue_type_;
};

// ---------------------------------------------------------------------------
// Test fixture: owns a Nextion instance and a list of mock components / queue
// entries so the owned pointers survive until the test ends.
// ---------------------------------------------------------------------------
class DeliverQueueResponse : public ::testing::Test {
 protected:
  void SetUp() override {
    // Nextion is final, but we only need to access (protected) queue methods
    // via FRIEND_TEST.  We do NOT call setup() because we are not connecting
    // to real hardware — we directly manipulate the queue.
    nextion_ = std::make_unique<Nextion>();
  }

  void TearDown() override {
    // The Nextion destructor does NOT free the queue entries' component
    // pointers (they are owned by the test), so clear the queue first.
    for (auto *nb : nextion_->nextion_queue_) {
      delete nb;  // NOLINT(cppcoreguidelines-owning-memory)
    }
    nextion_->nextion_queue_.clear();
    nextion_.reset();
    components_.clear();
  }

  // --- helpers to build the queue ---

  void add_no_result_() {
    auto *nb = new NextionQueue();  // NOLINT(cppcoreguidelines-owning-memory)
    auto *comp = new MockSensorComponent("no_result", NextionQueueType::NO_RESULT);
    nb->component = comp;
    nb->queue_time = 0;
    nextion_->nextion_queue_.push_back(nb);
    components_.push_back(std::unique_ptr<MockSensorComponent>(comp));
  }

  void add_sensor_(const std::string &name, bool mark_pending = false) {
    auto *nb = new NextionQueue();  // NOLINT(cppcoreguidelines-owning-memory)
    auto *comp = new MockSensorComponent(name, NextionQueueType::SENSOR);
    nb->component = comp;
    nb->queue_time = 0;
    if (mark_pending) {
      nb->pending_command = "get " + name;
    }
    nextion_->nextion_queue_.push_back(nb);
    components_.push_back(std::unique_ptr<MockSensorComponent>(comp));
  }

  void add_text_sensor_(const std::string &name, bool mark_pending = false) {
    auto *nb = new NextionQueue();  // NOLINT(cppcoreguidelines-owning-memory)
    auto *comp = new MockSensorComponent(name, NextionQueueType::TEXT_SENSOR);
    nb->component = comp;
    nb->queue_time = 0;
    if (mark_pending) {
      nb->pending_command = "get " + name;
    }
    nextion_->nextion_queue_.push_back(nb);
    components_.push_back(std::unique_ptr<MockSensorComponent>(comp));
  }

  void add_binary_sensor_(const std::string &name) {
    auto *nb = new NextionQueue();  // NOLINT(cppcoreguidelines-owning-memory)
    auto *comp = new MockSensorComponent(name, NextionQueueType::BINARY_SENSOR);
    nb->component = comp;
    nb->queue_time = 0;
    nextion_->nextion_queue_.push_back(nb);
    components_.push_back(std::unique_ptr<MockSensorComponent>(comp));
  }

  void add_switch_(const std::string &name) {
    auto *nb = new NextionQueue();  // NOLINT(cppcoreguidelines-owning-memory)
    auto *comp = new MockSensorComponent(name, NextionQueueType::SWITCH);
    nb->component = comp;
    nb->queue_time = 0;
    nextion_->nextion_queue_.push_back(nb);
    components_.push_back(std::unique_ptr<MockSensorComponent>(comp));
  }

  std::unique_ptr<Nextion> nextion_;
  std::vector<std::unique_ptr<MockSensorComponent>> components_;
};

// ==========================================================================
// Test 1: [NO_RESULT, SENSOR] — numeric response should skip the NO_RESULT
//         and deliver to the SENSOR.
// ==========================================================================
TEST_F(DeliverQueueResponse, SkipNoResultSecondSensorGetsValue) {
  add_no_result_();
  add_sensor_("temp");

  nextion_->set_numeric_return(42);

  // NO_RESULT should still be in the queue (its 0x01 ACK hasn't arrived)
  ASSERT_EQ(nextion_->nextion_queue_.size(), 1);
  EXPECT_EQ(nextion_->nextion_queue_.front()->component->get_queue_type(), NextionQueueType::NO_RESULT);

  // The sensor should have received the value
  auto *sensor = dynamic_cast<MockSensorComponent *>(components_[1].get());
  ASSERT_NE(sensor, nullptr);
  EXPECT_TRUE(sensor->state_set_);
  EXPECT_EQ(sensor->int_value_, 42);
}

// ==========================================================================
// Test 2: [NO_RESULT, TEXT_SENSOR] — string response (0x70) should skip
//         the NO_RESULT and deliver to the TEXT_SENSOR.
// ==========================================================================
TEST_F(DeliverQueueResponse, SkipNoResultTextSensorGetsString) {
  add_no_result_();
  add_text_sensor_("status");

  const std::string expected = "online";
  nextion_->set_string_return(expected);

  // NO_RESULT should still be in the queue
  ASSERT_EQ(nextion_->nextion_queue_.size(), 1);
  EXPECT_EQ(nextion_->nextion_queue_.front()->component->get_queue_type(), NextionQueueType::NO_RESULT);

  // The text sensor should have received the value
  auto *ts = dynamic_cast<MockSensorComponent *>(components_[1].get());
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->state_set_);
  EXPECT_EQ(ts->str_value_, expected);
}

// ==========================================================================
// Test 3: [NO_RESULT, SENSOR (unsent)] — under command spacing, the scanner
//         should skip the unsent entry and keep scanning.  No delivery.
// ==========================================================================
TEST_F(DeliverQueueResponse, SkipUnsentEntryUnderCommandSpacing) {
  add_no_result_();
  add_sensor_("temp", true);  // pending_command = "get temp"

  nextion_->set_numeric_return(99);

  // Both entries should remain in the queue (unsent entry was skipped,
  // no matching entry was consumed)
  ASSERT_EQ(nextion_->nextion_queue_.size(), 2);

  auto it = nextion_->nextion_queue_.begin();
  EXPECT_EQ((*it)->component->get_queue_type(), NextionQueueType::NO_RESULT);
  ++it;
  EXPECT_EQ((*it)->component->get_queue_type(), NextionQueueType::SENSOR);

  // The sensor should NOT have received the value (it was skipped)
  auto *sensor = dynamic_cast<MockSensorComponent *>(components_[1].get());
  ASSERT_NE(sensor, nullptr);
  EXPECT_FALSE(sensor->state_set_);
}

// ==========================================================================
// Test 4: [SENSOR] numeric response but only TEXT_SENSOR queued — mismatched
//         type causes the entry to be removed and a no-match log emitted.
// ==========================================================================
TEST_F(DeliverQueueResponse, RemoveMismatchedEntry) {
  add_text_sensor_("status");

  // Send a numeric response (0x71) — TEXT_SENSOR doesn't match numeric types
  nextion_->set_numeric_return(100);

  // The text sensor entry should have been removed (mismatched type)
  EXPECT_TRUE(nextion_->nextion_queue_.empty());

  // Its state should NOT have been set
  auto *ts = dynamic_cast<MockSensorComponent *>(components_[0].get());
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->state_set_);
}

}  // namespace esphome::nextion::testing
