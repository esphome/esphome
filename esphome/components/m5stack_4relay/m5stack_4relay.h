#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace m5stack_4relay {

class M5STACK4RELAYSwitchComponent;
class M5STACK4RELAYChannel : public switch_::Switch {
 public:
  M5STACK4RELAYChannel(M5STACK4RELAYSwitchComponent *parent, uint8_t channel) : parent_(parent), channel_(channel) {}

 protected:
  void write_state(bool state) override;

  M5STACK4RELAYSwitchComponent *parent_;
  uint8_t channel_;
};

class M5STACK4RELAYSwitchComponent : public Component, public i2c::I2CDevice {
 public:
  const uint8_t MODE_CONTROL_REG = 0x10;
  const uint8_t RELAY_CONTROL_REG = 0x11;
  uint8_t component_status;
  M5STACK4RELAYSwitchComponent() {}
  M5STACK4RELAYChannel *create_channel(uint8_t channel);

  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  friend M5STACK4RELAYChannel;

  void get_status(uint8_t *state);

  bool set_channel_value_(uint8_t channel, bool state);

  bool read_bytes_(uint8_t a_register, uint8_t *data, uint8_t len, uint32_t conversion) {
    return this->parent_->raw_receive(this->address_, data, len);
  }

  float value_;
};

}  // namespace m5stack_4relay
}  // namespace esphome
