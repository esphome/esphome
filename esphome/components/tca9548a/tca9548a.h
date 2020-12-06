#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tca9548a {

class TCA9548AComponent : public Component, public i2c::I2CMultiplexer {
 public:

  void set_scan(bool scan) { scan_ = scan; }
  void setup();
  void dump_config();
  void update();
  bool set_channel(uint8_t channelno);

 protected:
  bool scan_;

};



}  // namespace tca9548a
}  // namespace esphome
