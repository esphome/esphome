#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/rs485/rs485.h"

namespace esphome {
namespace dooya {

static const uint8_t START_CODE = 0x55;

enum COMMAND : uint8_t {
  READ = 0x01,
  WRITE = 0x02,
  CONTROL = 0x03,
};

enum READ_TYPE : uint8_t {
  GET_POSITION = 0x02,
  GET_STATUS = 0x05,
};

enum CONTROL_TYPE : uint8_t {
  OPEN = 0x01,
  CLOSE = 0x02,
  STOP = 0x03,
  SET_POSITION = 0x04,
};

class Dooya : public cover::Cover, public Component, public rs485::RS485Device {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_address(uint16_t address) {
    int header[3] = {START_CODE, (int)(address >> 8), (int)(address & 0xFF)};
    memcpy(&this->header_, &header, 3);
    this->address_[0] = (uint8_t)(address >> 8);
    this->address_[1] = (uint8_t)(address & 0xFF);
  }
  void on_rs485_data(const std::vector<uint8_t> &data) override;
  cover::CoverTraits get_traits() override;
  void send_command_(uint8_t &data);


 protected:
  void control(const cover::CoverCall &call) override;

  uint8_t address_[2] = {0xFE, 0xFE};
  uint32_t last_status_check_ = 0;
  uint8_t current_request_ = 0;

}  // namespace pzemac
}  // namespace esphome
