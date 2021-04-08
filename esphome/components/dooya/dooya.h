#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/rs485/rs485.h"

namespace esphome {
namespace dooya {

static const uint8_t START_CODE = 0x55;
static const uint8_t DEF_ADDR = 0xFE;

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

class Dooya : public cover::Cover, public PollingComponent, public rs485::RS485Device {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_address(uint16_t address) {
    uint8_t address_h = (uint8_t)(address >> 8);
    uint8_t address_l = (uint8_t)(address & 0xFF);
    this->header_ = {(uint8_t *) &START_CODE, &address_h, &address_l};
  }
  void on_rs485_data(const std::vector<uint8_t> &data) override;
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void send_command_(const uint8_t *data, uint8_t len);

  uint8_t current_request_{GET_STATUS};
  uint8_t last_published_op_;
  float last_published_pos_;
};

}  // namespace dooya
}  // namespace esphome
