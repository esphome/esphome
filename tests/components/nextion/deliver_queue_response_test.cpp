#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "esphome/components/nextion/nextion.h"
#include "esphome/core/helpers.h"

namespace esphome::nextion::testing {

// Explicit instantiation skips access checks, which lets the test reach the queue and the response
// handlers without making them public on the component.
template<std::list<NextionQueue *> Nextion::*Q, void (Nextion::*N)(int), void (Nextion::*S)(const std::string &),
         NextionQueue *(Nextion::*M)(const std::string &), void (Nextion::*R)(NextionQueue *)>
struct NextionAccess {
  friend std::list<NextionQueue *> &queue(Nextion &nextion) { return nextion.*Q; }
  friend void numeric_return(Nextion &nextion, int value) { (nextion.*N)(value); }
  friend void string_return(Nextion &nextion, const std::string &value) { (nextion.*S)(value); }
  friend NextionQueue *make_no_result_entry(Nextion &nextion, const std::string &name) { return (nextion.*M)(name); }
  friend void release_queue_entry(Nextion &nextion, NextionQueue *entry) { (nextion.*R)(entry); }
};
std::list<NextionQueue *> &queue(Nextion &nextion);
void numeric_return(Nextion &nextion, int value);
void string_return(Nextion &nextion, const std::string &value);
NextionQueue *make_no_result_entry(Nextion &nextion, const std::string &name);
void release_queue_entry(Nextion &nextion, NextionQueue *entry);
template struct NextionAccess<&Nextion::nextion_queue_, &Nextion::set_numeric_return_, &Nextion::set_string_return_,
                              &Nextion::make_no_result_entry_, &Nextion::release_queue_entry_>;

/// Stands in for a sensor, switch or text sensor and records the value it receives
class MockComponent : public NextionComponentBase {
 public:
  MockComponent(const std::string &name, NextionQueueType type) : type_(type) { this->set_variable_name(name); }

  NextionQueueType get_queue_type() const override { return this->type_; }
  void set_state_from_int(int state_value, bool publish, bool send_to_nextion) override {
    this->received = true;
    this->int_value = state_value;
  }
  void set_state_from_string(const std::string &state_value, bool publish, bool send_to_nextion) override {
    this->received = true;
    this->str_value = state_value;
  }

  bool received{false};
  int int_value{0};
  std::string str_value;

 protected:
  NextionQueueType type_;
};

class DeliverQueueResponse : public ::testing::Test {
 protected:
  void TearDown() override {
    for (auto *entry : queue(this->nextion_))
      release_queue_entry(this->nextion_, entry);
    queue(this->nextion_).clear();
  }

  /// Queues a query the way add_to_get_queue() does. An unsent query still waits for command spacing.
  MockComponent *add_query_(const std::string &name, NextionQueueType type, bool sent = true) {
    auto *component = this->components_.emplace_back(std::make_unique<MockComponent>(name, type)).get();
    auto *entry = RAMAllocator<NextionQueue>().allocate(1);
    new (entry) NextionQueue;
    entry->component = component;
    if (!sent)
      entry->pending_command = "get " + name;
    queue(this->nextion_).push_back(entry);
    return component;
  }

  /// Queues a sent command that waits for its 0x01 ACK
  void add_no_result_(const std::string &name) {
    queue(this->nextion_).push_back(make_no_result_entry(this->nextion_, name));
  }

  /// Names of the queued entries, front first
  std::vector<std::string> queued_() {
    std::vector<std::string> names;
    for (auto *entry : queue(this->nextion_))
      names.push_back(entry->component == nullptr ? "<null>" : entry->component->get_variable_name());
    return names;
  }

  Nextion nextion_;
  std::vector<std::unique_ptr<MockComponent>> components_;
};

// The regression: a numeric return must not consume a command that is still waiting for its ACK
TEST_F(DeliverQueueResponse, NumericSkipsNoResult) {
  this->add_no_result_("cmd");
  auto *sensor = this->add_query_("temp", NextionQueueType::SENSOR);

  numeric_return(this->nextion_, 42);

  EXPECT_TRUE(sensor->received);
  EXPECT_EQ(sensor->int_value, 42);
  EXPECT_EQ(this->queued_(), std::vector<std::string>{"cmd"});
}

TEST_F(DeliverQueueResponse, StringSkipsNoResult) {
  this->add_no_result_("cmd");
  auto *text = this->add_query_("status", NextionQueueType::TEXT_SENSOR);

  string_return(this->nextion_, "online");

  EXPECT_TRUE(text->received);
  EXPECT_EQ(text->str_value, "online");
  EXPECT_EQ(this->queued_(), std::vector<std::string>{"cmd"});
}

TEST_F(DeliverQueueResponse, NumericGoesToBinarySensorAndSwitch) {
  auto *binary = this->add_query_("bin", NextionQueueType::BINARY_SENSOR);
  auto *sw = this->add_query_("sw", NextionQueueType::SWITCH);

  numeric_return(this->nextion_, 1);
  EXPECT_TRUE(binary->received);
  EXPECT_EQ(binary->int_value, 1);
  EXPECT_FALSE(sw->received);

  numeric_return(this->nextion_, 0);
  EXPECT_TRUE(sw->received);
  EXPECT_EQ(sw->int_value, 0);
  EXPECT_TRUE(this->queued_().empty());
}

// With command spacing, a query can be queued before its command is sent; the display cannot be answering it yet
TEST_F(DeliverQueueResponse, NumericSkipsUnsentQuery) {
  auto *unsent = this->add_query_("unsent", NextionQueueType::SENSOR, false);
  auto *sent = this->add_query_("sent", NextionQueueType::SENSOR);

  numeric_return(this->nextion_, 7);

  EXPECT_FALSE(unsent->received);
  EXPECT_TRUE(sent->received);
  EXPECT_EQ(this->queued_(), std::vector<std::string>{"unsent"});
}

TEST_F(DeliverQueueResponse, NumericWithoutSentQueryLeavesQueue) {
  this->add_no_result_("cmd");
  auto *unsent = this->add_query_("temp", NextionQueueType::SENSOR, false);

  numeric_return(this->nextion_, 99);

  EXPECT_FALSE(unsent->received);
  EXPECT_EQ(this->queued_(), (std::vector<std::string>{"cmd", "temp"}));
}

// The display answers queries in order, so a wrong-type return still belongs to the first sent query.
// It must consume only that query and never hand the value to a later one.
TEST_F(DeliverQueueResponse, WrongTypeConsumesOnlyFirstQuery) {
  auto *text = this->add_query_("status", NextionQueueType::TEXT_SENSOR);
  auto *sensor = this->add_query_("temp", NextionQueueType::SENSOR);

  numeric_return(this->nextion_, 100);

  EXPECT_FALSE(text->received);
  EXPECT_FALSE(sensor->received);
  EXPECT_EQ(this->queued_(), std::vector<std::string>{"temp"});
}

TEST_F(DeliverQueueResponse, StringWrongTypeConsumesOnlyFirstQuery) {
  auto *sensor = this->add_query_("temp", NextionQueueType::SENSOR);
  auto *text = this->add_query_("status", NextionQueueType::TEXT_SENSOR);

  string_return(this->nextion_, "online");

  EXPECT_FALSE(sensor->received);
  EXPECT_FALSE(text->received);
  EXPECT_EQ(this->queued_(), std::vector<std::string>{"status"});
}

TEST_F(DeliverQueueResponse, InvalidEntryIsDropped) {
  auto *invalid = RAMAllocator<NextionQueue>().allocate(1);
  new (invalid) NextionQueue;
  queue(this->nextion_).push_back(invalid);
  auto *sensor = this->add_query_("temp", NextionQueueType::SENSOR);

  numeric_return(this->nextion_, 5);

  EXPECT_TRUE(sensor->received);
  EXPECT_TRUE(this->queued_().empty());
}

}  // namespace esphome::nextion::testing
