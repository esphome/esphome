#pragma once
#include <cmath>
#include <string>
#include <gtest/gtest.h>
#include "esphome/components/dfrobot_sen0609/dfrobot_sen0609.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"

namespace esphome::dfrobot_sen0609::testing {

// Records what the hub writes and hands it what the test queued as module output.
class FakeUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    this->tx.append(reinterpret_cast<const char *>(data), len);
  }
  bool peek_byte(uint8_t *data) override {
    if (this->available() == 0)
      return false;
    *data = this->rx[this->rx_pos];
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->available() < len)
      return false;
    memcpy(data, this->rx.data() + this->rx_pos, len);
    this->rx_pos += len;
    return true;
  }
  size_t available() override { return this->rx.size() - this->rx_pos; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

  std::string tx;
  std::string rx;
  size_t rx_pos{0};
};

class SEN0609Test : public ::testing::Test {
 protected:
  explicit SEN0609Test(RunMode mode = RunMode::RUN_MODE_PRESENCE) : hub_(mode) {}

  void SetUp() override {
    this->hub_.set_uart_parent(&this->uart_);
    this->hub_.set_has_target_binary_sensor(&this->has_target_);
  }

  // Runs the hub's loop `ms` later than the previous run
  void run_(uint32_t ms = 16) {
    this->now_ += ms;
    LoopBlockingGuard guard{nullptr, nullptr, this->now_};
    this->hub_.loop();
  }

  void feed_(const std::string &text) {
    this->uart_.rx += text;
    this->run_();
  }

  // Runs the loop until the hub writes a full command line, and returns it without the line end
  std::string next_command_(uint32_t max_ms = 5000) {
    for (uint32_t waited = 0; waited <= max_ms; waited += 16) {
      const size_t end = this->uart_.tx.find("\r\n");
      if (end != std::string::npos) {
        std::string command = this->uart_.tx.substr(0, end);
        this->uart_.tx.erase(0, end + 2);
        return command;
      }
      this->run_();
    }
    return "";
  }

  // Answers a command the way the module does: echo, optional lines, Done and the prompt
  void answer_(const std::string &command, const std::string &lines = "") {
    this->feed_(command + "\r\n" + lines + "Done\r\nDFRobot:/>");
  }

  // Expects `command` next and answers it
  void expect_and_answer_(const std::string &command, const std::string &lines = "") {
    ASSERT_EQ(this->next_command_(), command);
    this->answer_(command, lines);
  }

  FakeUART uart_;
  DFRobotSEN0609Component hub_;
  binary_sensor::BinarySensor has_target_;
  uint32_t now_{1000};
};

}  // namespace esphome::dfrobot_sen0609::testing
