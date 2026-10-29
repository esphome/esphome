#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
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

static_assert(sizeof(EzoCommand) == 5, "a queue slot holds the command id and one float");

TEST_F(EzoQueueTest, WireTextOfEachCommand) {
  this->ezo_.get_device_information();
  this->ezo_.set_sleep();
  this->ezo_.get_slope();
  this->ezo_.get_t();
  this->ezo_.set_t(21.5f);
  this->ezo_.set_tempcomp_value(-5.256f);
  this->ezo_.get_calibration();
  this->ezo_.clear_calibration();
  this->ezo_.set_calibration_point_low(4.0f);
  this->ezo_.set_calibration_point_mid(7.0f);
  this->ezo_.set_calibration_point_high(10.0f);
  this->ezo_.set_calibration_generic(1413.0f);
  this->run_all_();
  this->ezo_.get_led_state();
  this->ezo_.set_led_state(false);
  this->ezo_.set_led_state(true);
  this->ezo_.send_custom("Name,?");
  this->ezo_.get_state();
  this->ezo_.set_address(100);
  this->run_all_();
  EXPECT_EQ(this->bus_.writes,
            (std::vector<std::string>{"i", "Sleep", "Slope,?", "T,?", "T,21.50", "T,-5.26", "Cal,?", "Cal,clear",
                                      "Cal,LOW,4.00", "Cal,MID,7.00", "Cal,HIGH,10.00", "Cal,1413.00", "L,?", "L,0",
                                      "L,1", "Name,?", "R", "I2C,100"}));
}

// An explicit read waits its turn, so it sees the compensation queued before it.
TEST_F(EzoQueueTest, ExplicitReadRunsInOrder) {
  this->ezo_.set_t(25.0f);
  this->ezo_.get_state();
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"T,25.00", "R"}));
  EXPECT_FLOAT_EQ(this->ezo_.state, 7.0f);
}

// The finished command frees its slot before its callbacks run.
TEST_F(EzoQueueTest, CallbackCanQueueIntoFullQueue) {
  bool queued = false;
  this->ezo_.add_led_state_callback([this, &queued](bool) {
    if (!queued) {
      queued = true;
      this->ezo_.get_slope();
    }
  });
  this->bus_.reply = "?L,1";
  for (uint8_t i = 0; i < EZO_COMMAND_QUEUE_SIZE; i++)
    this->ezo_.get_led_state();
  this->run_all_();
  ASSERT_EQ(this->bus_.writes.size(), EZO_COMMAND_QUEUE_SIZE + 1u);
  EXPECT_EQ(this->bus_.writes.back(), "Slope,?");
}

TEST_F(EzoQueueTest, CustomCallbackCanSendCustom) {
  bool sent = false;
  this->ezo_.add_custom_callback([this, &sent](const std::string &) {
    if (!sent) {
      sent = true;
      this->ezo_.send_custom("Find");
    }
  });
  this->ezo_.send_custom("Status");
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"Status", "Find"}));
}

TEST_F(EzoQueueTest, LongestDocumentedCommandFits) {
  this->ezo_.send_custom("Import, 59 6F 75 20 61 72");
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"Import, 59 6F 75 20 61 72"}));
}

TEST_F(EzoQueueTest, FullQueueDropsNewCommands) {
  for (uint8_t i = 0; i < EZO_COMMAND_QUEUE_SIZE; i++)
    this->ezo_.set_t(static_cast<float>(i));
  this->ezo_.get_led_state();  // one more than fits
  this->ezo_.update();         // reads do not take a slot
  this->run_all_();
  std::vector<std::string> expected{"R"};
  for (uint8_t i = 0; i < EZO_COMMAND_QUEUE_SIZE; i++) {
    char text[16];
    snprintf(text, sizeof(text), "T,%u.00", i);
    expected.emplace_back(text);
  }
  EXPECT_EQ(this->bus_.writes, expected);
}

TEST_F(EzoQueueTest, OneCustomCommandAtATime) {
  this->ezo_.send_custom("Name,?");
  this->ezo_.send_custom("Status");  // the text buffer is taken
  this->ezo_.get_led_state();
  this->run_all_();
  this->ezo_.send_custom("Find");  // free again once the first one finished
  this->run_all_();
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"Name,?", "L,?", "Find"}));
}

TEST_F(EzoQueueTest, CustomCommandDroppedByFullQueueFreesTheText) {
  for (uint8_t i = 0; i < EZO_COMMAND_QUEUE_SIZE; i++)
    this->ezo_.get_led_state();
  this->ezo_.send_custom("Name,?");
  this->run_all_();
  this->ezo_.send_custom("Status");
  this->run_all_();
  ASSERT_EQ(this->bus_.writes.size(), EZO_COMMAND_QUEUE_SIZE + 1u);
  EXPECT_EQ(this->bus_.writes.back(), "Status");
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
