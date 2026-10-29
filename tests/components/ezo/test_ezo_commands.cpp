#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "esphome/components/ezo/ezo.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::ezo::testing {

static constexpr uint8_t ADDRESS = 99;

// Records each command written and answers each read with a success code and the reply set for the last command.
class EzoCommandBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    this->addresses.push_back(address);
    if (size != 0)
      this->writes.emplace_back(reinterpret_cast<const char *>(data), size);
    if (out_size != 0) {
      memset(out, 0, out_size);
      out[0] = 1;
      auto it = this->replies.find(this->writes.empty() ? std::string() : this->writes.back());
      const std::string &reply = it == this->replies.end() ? this->default_reply : it->second;
      memcpy(out + 1, reply.data(), std::min(reply.size(), out_size - 1));
    }
    return i2c::ERROR_OK;
  }

  std::vector<std::string> writes;
  std::vector<uint8_t> addresses;
  std::map<std::string, std::string> replies;
  std::string default_reply{"7.00"};
};

class EzoCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ezo_.set_i2c_bus(&this->bus_);
    this->ezo_.set_i2c_address(ADDRESS);
  }

  // Runs the loop on the real clock until `count` commands were written and answered.
  void run_until_written_(size_t count) {
    // Longer than the slowest reply wait (900 ms), so the last reply has arrived.
    static constexpr uint32_t SETTLE_MS = 1000;
    const uint32_t start = millis();
    uint32_t written_at = start;
    while (millis() - start < 30000) {
      {
        LoopBlockingGuard guard{nullptr, nullptr, millis()};
        this->ezo_.loop();
      }
      if (this->bus_.writes.size() < count) {
        written_at = millis();
      } else if (millis() - written_at > SETTLE_MS) {
        break;
      }
      delay(5);
    }
  }

  EzoCommandBus bus_;
  EZOSensor ezo_;
};

TEST_F(EzoCommandTest, WireTextOfEachCommand) {
  this->ezo_.get_device_information();
  this->ezo_.set_sleep();
  this->ezo_.get_slope();
  this->ezo_.get_t();
  this->ezo_.set_t(21.5f);
  this->ezo_.get_calibration();
  this->ezo_.clear_calibration();
  this->ezo_.set_calibration_point_low(4.0f);
  this->run_until_written_(8);
  this->ezo_.set_tempcomp_value(-5.256f);
  this->ezo_.set_calibration_point_mid(7.0f);
  this->ezo_.set_calibration_point_high(10.0f);
  this->ezo_.set_calibration_generic(1413.0f);
  this->ezo_.get_led_state();
  this->ezo_.set_led_state(false);
  this->ezo_.set_led_state(true);
  this->ezo_.send_custom("Name,?");
  this->run_until_written_(16);
  this->ezo_.get_state();
  this->run_until_written_(17);
  EXPECT_EQ(this->bus_.writes,
            (std::vector<std::string>{"i", "Sleep", "Slope,?", "T,?", "T,21.50", "Cal,?", "Cal,clear", "Cal,LOW,4.00",
                                      "T,-5.26", "Cal,MID,7.00", "Cal,HIGH,10.00", "Cal,1413.00", "L,?", "L,0", "L,1",
                                      "Name,?", "R"}));
}

TEST_F(EzoCommandTest, RepliesReachTheirCallbacks) {
  std::vector<std::string> got;
  this->ezo_.add_device_infomation_callback([&got](std::string x) { got.push_back("info " + x); });
  this->ezo_.add_slope_callback([&got](std::string x) { got.push_back("slope " + x); });
  this->ezo_.add_calibration_callback([&got](std::string x) { got.push_back("cal " + x); });
  this->ezo_.add_t_callback([&got](std::string x) { got.push_back("t " + x); });
  this->ezo_.add_led_state_callback([&got](bool on) { got.push_back(on ? "led on" : "led off"); });
  this->ezo_.add_custom_callback([&got](std::string x) { got.push_back("custom " + x); });
  this->bus_.replies = {
      {"i", "?I,pH,2.16"}, {"Slope,?", "?Slope,99.7,100.3,-0.89"}, {"Cal,?", "?Cal,2"}, {"T,?", "?T,19.5"},
      {"L,?", "?L,1"},     {"Status", "?Status,P,5.038"},          {"R", "7.25,1.5"}};
  this->ezo_.get_device_information();
  this->ezo_.get_slope();
  this->ezo_.get_calibration();
  this->ezo_.get_t();
  this->ezo_.get_led_state();
  this->ezo_.send_custom("Status");
  this->ezo_.get_state();
  this->run_until_written_(7);
  EXPECT_EQ(got, (std::vector<std::string>{"info pH,2.16", "slope 99.7,100.3,-0.89", "cal 2", "t 19.5", "led on",
                                           "custom ?Status,P,5.038"}));
  // A read publishes the first of several values.
  EXPECT_FLOAT_EQ(this->ezo_.state, 7.25f);
}

TEST_F(EzoCommandTest, AddressChangeAppliesToLaterCommands) {
  this->ezo_.set_address(100);
  this->ezo_.get_led_state();
  this->run_until_written_(2);
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"I2C,100", "L,?"}));
  // I2C,100 goes to the old address with no reply read; L,? is written and read at the new one.
  EXPECT_EQ(this->bus_.addresses, (std::vector<uint8_t>{ADDRESS, 100, 100}));
}

TEST_F(EzoCommandTest, InvalidAddressIsIgnored) {
  this->ezo_.set_address(0);
  this->ezo_.set_address(128);
  this->ezo_.get_led_state();
  this->run_until_written_(1);
  EXPECT_EQ(this->bus_.writes, (std::vector<std::string>{"L,?"}));
  EXPECT_EQ(this->bus_.addresses, (std::vector<uint8_t>{ADDRESS, ADDRESS}));
}

}  // namespace esphome::ezo::testing
