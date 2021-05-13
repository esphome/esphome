#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/uart_multi/uart_multi.h"

namespace esphome {
namespace gm40 {

static const uint8_t START_CODE_H = 0x5A;
static const uint8_t START_CODE_L = 0xA5;

enum Command : uint8_t {
  READ = 0x03,
  CONTROL = 0x06,
};

enum ControlType : uint8_t {
  OPEN = 0x01,
  CLOSE = 0x02,
  STOP = 0x03,
  SET_POSITION = 0x04,
};

class GM40 : public cover::Cover, public Component, public uart_multi::UARTMultiDevice {
 public:
  void dump_config() override;

  void set_address(uint8_t address) { this->address_ = address; }
  void send_update() override;
  void on_uart_multi_byte(uint8_t byte) override;
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void process_response_();
  void process_status_();
  void send_command_(const uint8_t *data, uint8_t len);

  uint8_t address_{0x00};
  std::vector<uint8_t> rx_buffer_;
  float target_position_{0};
};

}  // namespace gm40
}  // namespace esphome
