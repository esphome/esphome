/// @file uart_tester.h
/// @author DrCoolZic
/// @brief  uart_tester classes interface

#pragma once
#include <bitset>
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace uart_tester {

/// @brief size of the buffer
constexpr size_t BUFFER_SIZE = 128;

class UARTTester : public PollingComponent, public uart::UARTDevice {
 public:
  void set_name(std::string name) { this->name_ = name; }
  const char *get_name() { return (this->name_).c_str(); }

  void loop() override;
  void setup() override;
  void update() override {}  // TODO
  void dump_config() override;
  // float get_setup_priority() const override { return setup_priority::IO; }

 protected:
  /// @brief send a frame on an uart
  /// @param uart
  /// @param length
  void uart_send_frame_();

  /// @brief receive a frame on an uart
  /// @param uart
  void uart_receive_frame_();
  void uart_receive_frame_1_by_1_();

  std::vector<uint8_t> input_buffer{std::vector<uint8_t>(BUFFER_SIZE)};
  std::vector<uint8_t> output_buffer{std::vector<uint8_t>(BUFFER_SIZE)};

  enum State {
    OFF,
    ON,
    START_FRAME_RECEIVED,
    END_FRAME_RECEIVED,
  } state_{OFF};

  ///< set to true when the wk2132 is initialized
  bool initialized_{false};
  std::string name_;
};

}  // namespace uart_tester
}  // namespace esphome
