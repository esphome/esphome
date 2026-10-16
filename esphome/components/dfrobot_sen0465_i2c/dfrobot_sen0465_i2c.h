#pragma once

#include "esphome/components/dfrobot_sen0465_base/dfrobot_sen0465_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::dfrobot_sen0465_i2c {

class DFRobotSEN0465I2CComponent final : public dfrobot_sen0465_base::DFRobotSEN0465Component, public i2c::I2CDevice {
 public:
  using DFRobotSEN0465Component::DFRobotSEN0465Component;

  void dump_config() override;

 protected:
  bool write_frame_(const uint8_t *frame) override;
  void read_response_() override;
};

}  // namespace esphome::dfrobot_sen0465_i2c
