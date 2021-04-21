#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/uart_multi/uart_multi.h"

namespace esphome {
namespace dooya {

static const uint8_t START_CODE = 0x55;

enum Command : uint8_t {
  READ = 0x01,
  WRITE = 0x02,
  CONTROL = 0x03,
};

enum ReadType : uint8_t {
  GET_POSITION = 0x02,
  GET_STATUS = 0x05,
};

enum ControlType : uint8_t {
  OPEN = 0x01,
  CLOSE = 0x02,
  STOP = 0x03,
  SET_POSITION = 0x04,
};

class Dooya : public cover::Cover, public PollingComponent, public uart_multi::UARTMultiDevice {
 public:
  void update() override;
  void dump_config() override;

  void set_address(uint16_t address) {
    this->address_[0] = (uint8_t)(address >> 8);
    this->address_[1] = (uint8_t)(address & 0xFF);
  }
  void on_uart_multi_byte(uint8_t byte) override;
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void process_response_();
  void process_status_();
  void send_command_(const uint8_t *data, uint8_t len);

  std::vector<uint8_t> rx_buffer_;
  uint8_t address_[2] = {0xFE, 0xFE};
  uint8_t current_request_{GET_STATUS};
};

}  // namespace dooya
}  // namespace esphome
