/// @file uart_tester.h
/// @author DrCoolZic
/// @brief  uart_tester classes interface

#pragma once
#include <bitset>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace uart_tester {

/// @brief size of the buffer
constexpr size_t BUFFER_SIZE = 128;

class UARTTester : public PollingComponent, public uart::UARTDevice {
 public:
  void set_name(std::string name) { this->name_ = name; }
  const char *get_name() { return (this->name_).c_str(); }
  void set_mode(int mode) { this->mode_ = mode; }

  void loop() override;
  void setup() override;
  void update() override {}
  void dump_config() override;

 protected:
  void uart_send_frame_();
  void uart_receive_frame_();
  void uart_receive_frame_1_by_1_();

  std::bitset<8> mode_;
  std::vector<uint8_t> input_buffer{std::vector<uint8_t>(BUFFER_SIZE)};
  std::vector<uint8_t> output_buffer{std::vector<uint8_t>(BUFFER_SIZE)};
  bool initialized_{false};
  std::string name_;
};

}  // namespace uart_tester
}  // namespace esphome
