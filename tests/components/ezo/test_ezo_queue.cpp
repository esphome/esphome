#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/ezo/ezo.h"
#include "esphome/core/application.h"

namespace esphome::ezo::testing {

static constexpr uint8_t ADDRESS = 99;

// Records each command written and answers every read with a success code and `reply`.
class EzoBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    this->addresses.push_back(address);
    if (size != 0)
      this->writes.emplace_back(reinterpret_cast<const char *>(data), size);
    if (out_size != 0) {
      memset(out, 0, out_size);
      out[0] = 1;
      memcpy(out + 1, this->reply.c_str(), std::min(this->reply.size(), out_size - 1));
    }
    return i2c::ERROR_OK;
  }

  std::vector<std::string> writes;
  std::vector<uint8_t> addresses;
  std::string reply{"7.00"};
};

class EzoQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ezo_.set_i2c_bus(&this->bus_);
    this->ezo_.set_i2c_address(ADDRESS);
  }

  // One loop pass, one second after the previous one, so every reply delay has passed.
  void step_() {
    this->now_ += 1000;
    LoopBlockingGuard guard{nullptr, nullptr, this->now_};
    this->ezo_.loop();
  }

  void run_all_() {
    for (int i = 0; i < 64; i++)
      this->step_();
  }

  EzoBus bus_;
  EZOSensor ezo_;
  uint32_t now_{0};
};

TEST_F(EzoQueueTest, CommandsRunInOrder) {
  this->ezo_.get_led_state();
  this->ezo_.get_slope();
  this->ezo_.get_calibration();
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"L,?", "Slope,?", "Cal,?"}));
}

TEST_F(EzoQueueTest, ReadRunsNextAndOnlyOnce) {
  this->ezo_.get_led_state();
  this->ezo_.get_slope();
  this->step_();  // "L,?" is in flight
  this->ezo_.update();
  this->ezo_.update();
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"L,?", "R", "Slope,?"}));
  EXPECT_FLOAT_EQ(this->ezo_.state, 7.0f);
}

TEST_F(EzoQueueTest, ReadsDoNotStarveOtherCommands) {
  this->ezo_.get_led_state();
  this->ezo_.get_slope();
  this->ezo_.update();
  this->step_();        // the read goes first since nothing was in flight
  this->ezo_.update();  // asked again while the first read is in flight
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"R", "L,?", "R", "Slope,?"}));
}

TEST_F(EzoQueueTest, FullQueueDropsNewCommands) {
  for (uint8_t i = 0; i < EZO_COMMAND_QUEUE_SIZE + 2; i++)
    this->ezo_.send_custom(std::string("C,") + static_cast<char>('a' + i));
  this->ezo_.update();  // reads do not take a slot
  this->run_all_();
  ASSERT_EQ(this->bus_.writes.size(), EZO_COMMAND_QUEUE_SIZE + 1u);
  EXPECT_EQ(this->bus_.writes.front(), "R");
  EXPECT_EQ(this->bus_.writes.back(), std::string("C,") + static_cast<char>('a' + EZO_COMMAND_QUEUE_SIZE - 1));
}

TEST_F(EzoQueueTest, OversizeCustomCommandIsRejected) {
  std::vector<std::string> replies;
  this->ezo_.add_custom_callback([&replies](const std::string &x) { replies.push_back(x); });
  this->bus_.reply = "ok";
  this->ezo_.send_custom(std::string(EZO_MAX_COMMAND_LENGTH + 1, 'A'));
  this->ezo_.send_custom(std::string(EZO_MAX_COMMAND_LENGTH, 'B'));
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{std::string(EZO_MAX_COMMAND_LENGTH, 'B')}));
  EXPECT_EQ(replies, (std::vector<std::string>{"ok"}));
}

TEST_F(EzoQueueTest, ReadKeepsOnlyTheFirstValue) {
  this->bus_.reply = "7.25,1.5";
  this->ezo_.update();
  this->run_all_();
  EXPECT_FLOAT_EQ(this->ezo_.state, 7.25f);
}

// The device can fill all 31 payload bytes with no terminator; the reply must still end there.
TEST_F(EzoQueueTest, FullLengthReplyIsTerminated) {
  std::vector<std::string> replies;
  this->ezo_.add_custom_callback([&replies](const std::string &x) { replies.push_back(x); });
  this->bus_.reply = std::string(31, 'x');
  this->ezo_.send_custom("Status");
  this->run_all_();
  EXPECT_EQ(replies, (std::vector<std::string>{std::string(31, 'x')}));
}

TEST_F(EzoQueueTest, AddressChangeNeedsNoReply) {
  this->ezo_.set_address(100);
  this->ezo_.get_led_state();
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"I2C,100", "L,?"}));
  // The I2C command goes to the old address, everything after it to the new one.
  ASSERT_GE(this->bus_.addresses.size(), 2u);
  EXPECT_EQ(this->bus_.addresses.front(), ADDRESS);
  EXPECT_EQ(this->bus_.addresses.back(), 100);
}

TEST_F(EzoQueueTest, QueuedAddressChangesEachApplyTheirOwn) {
  this->ezo_.set_address(100);
  this->ezo_.set_address(101);
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"I2C,100", "I2C,101"}));
  // The second change goes to the address set by the first.
  EXPECT_EQ(this->bus_.addresses, (std::vector<uint8_t>{ADDRESS, 100}));
}

}  // namespace esphome::ezo::testing
