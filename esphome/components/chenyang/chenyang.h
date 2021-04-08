#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/rs485/rs485.h"

namespace esphome {
namespace chenyang {

static const uint8_t DEF_ADDR = 0xFF;

enum StartCode : uint8_t {
  COMMAND = 0x01,
  RESPONSE = 0x02,
  STATUS = 0x03,
};

enum ReadType : uint8_t {
  GET_STATUS = 0x0F,
};

enum ControlType : uint8_t {
  OPEN = 0x00,
  CLOSE = 0x01,
  STOP = 0x02,
  SET_POSITION = 0x03,
};

class Chenyang : public cover::Cover, public PollingComponent, public rs485::RS485Device {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_address(uint8_t address) { this->header_ = {nullptr, &address}; }
  void on_rs485_data(const std::vector<uint8_t> &data) override;
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void send_command_(const uint8_t *data, uint8_t len);

  uint8_t last_published_op_;
  float last_published_pos_;
};

}  // namespace chenyang
}  // namespace esphome
