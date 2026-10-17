#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "esphome/components/nextion/nextion.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/helpers.h"

namespace esphome::nextion::testing {

// Explicit instantiation skips access checks, which lets the test reach the queue and the response
// handlers without making them public on the component.
template<std::list<NextionQueue *> Nextion::*Q, void (Nextion::*N)(int), void (Nextion::*S)(const std::string &),
         void (Nextion::*R)(NextionQueue *), bool NextionBase::*U>
struct NextionAccess {
  friend std::list<NextionQueue *> &queue(Nextion &nextion) { return nextion.*Q; }
  friend void numeric_return(Nextion &nextion, int value) { (nextion.*N)(value); }
  friend void string_return(Nextion &nextion, const std::string &value) { (nextion.*S)(value); }
  friend void release_queue_entry(Nextion &nextion, NextionQueue *entry) { (nextion.*R)(entry); }
  friend void mark_setup(Nextion &nextion) { nextion.*U = true; }
};
std::list<NextionQueue *> &queue(Nextion &nextion);
void numeric_return(Nextion &nextion, int value);
void string_return(Nextion &nextion, const std::string &value);
void release_queue_entry(Nextion &nextion, NextionQueue *entry);
void mark_setup(Nextion &nextion);
template struct NextionAccess<&Nextion::nextion_queue_, &Nextion::set_numeric_return_, &Nextion::set_string_return_,
                              &Nextion::release_queue_entry_, &Nextion::is_setup_>;

/// Accepts the commands the component writes; the display never answers
class FakeUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override { return false; }
  size_t available() override { return 0; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif  // defined(USE_ESP8266) || defined(USE_ESP32)

 protected:
  void check_logger_conflict() override {}
};

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
  void SetUp() override {
    this->nextion_.set_uart_parent(&this->uart_);
    mark_setup(this->nextion_);
  }

  void TearDown() override {
    for (auto *entry : queue(this->nextion_)) {
      if (entry != nullptr)
        release_queue_entry(this->nextion_, entry);
    }
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

  /// Names of the queued entries, front first. send_command() names its entries "command".
  std::vector<std::string> queued_() {
    std::vector<std::string> names;
    for (auto *entry : queue(this->nextion_)) {
      if (entry == nullptr || entry->component == nullptr) {
        names.emplace_back("<invalid>");
      } else {
        names.push_back(entry->component->get_variable_name());
      }
    }
    return names;
  }

  FakeUART uart_;
  Nextion nextion_;
  std::vector<std::unique_ptr<MockComponent>> components_;
};

// The regression: a numeric return must not consume a command that is still waiting for its ACK
TEST_F(DeliverQueueResponse, NumericSkipsNoResult) {
  this->nextion_.send_command("page 0");
  auto *sensor = this->add_query_("temp", NextionQueueType::SENSOR);

  numeric_return(this->nextion_, 42);

  EXPECT_TRUE(sensor->received);
  EXPECT_EQ(sensor->int_value, 42);
  EXPECT_EQ(this->queued_(), std::vector<std::string>{"command"});
}

TEST_F(DeliverQueueResponse, StringSkipsNoResult) {
  this->nextion_.send_command("page 0");
  auto *text = this->add_query_("status", NextionQueueType::TEXT_SENSOR);

  string_return(this->nextion_, "online");

  EXPECT_TRUE(text->received);
  EXPECT_EQ(text->str_value, "online");
  EXPECT_EQ(this->queued_(), std::vector<std::string>{"command"});
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
  this->nextion_.send_command("page 0");
  auto *unsent = this->add_query_("temp", NextionQueueType::SENSOR, false);

  numeric_return(this->nextion_, 99);

  EXPECT_FALSE(unsent->received);
  EXPECT_EQ(this->queued_(), (std::vector<std::string>{"command", "temp"}));
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

TEST_F(DeliverQueueResponse, InvalidEntriesAreDropped) {
  auto *no_component = RAMAllocator<NextionQueue>().allocate(1);
  new (no_component) NextionQueue;
  queue(this->nextion_).push_back(nullptr);
  queue(this->nextion_).push_back(no_component);
  auto *sensor = this->add_query_("temp", NextionQueueType::SENSOR);

  numeric_return(this->nextion_, 5);

  EXPECT_TRUE(sensor->received);
  EXPECT_TRUE(this->queued_().empty());
}

// The display answers a raw "get" with a value instead of 0x01, so that value removes it.
// The value must not reach a later query, and no orphan entry may stay behind for later ACKs to remove.
TEST_F(DeliverQueueResponse, RawGetTakesItsOwnValue) {
  this->nextion_.send_command("get n0.val");
  this->nextion_.send_command_printf("get %s", "t0.txt");
  auto *sensor = this->add_query_("temp", NextionQueueType::SENSOR);
  auto *text = this->add_query_("status", NextionQueueType::TEXT_SENSOR);

  numeric_return(this->nextion_, 3);
  string_return(this->nextion_, "raw");

  EXPECT_FALSE(sensor->received);
  EXPECT_FALSE(text->received);
  EXPECT_EQ(this->queued_(), (std::vector<std::string>{"temp", "status"}));
}

}  // namespace esphome::nextion::testing
