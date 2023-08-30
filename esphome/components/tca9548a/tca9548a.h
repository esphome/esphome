#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tca9548a {

class TCA9548AComponent;
class TCA9548AChannel : public i2c::I2CBus {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }
  void set_parent(TCA9548AComponent *parent) { parent_ = parent; }

  i2c::ErrorCode readv(uint8_t address, i2c::ReadBuffer *buffers, size_t cnt) override;
  i2c::ErrorCode writev(uint8_t address, i2c::WriteBuffer *buffers, size_t cnt, bool stop) override;

 protected:
  uint8_t channel_;
  TCA9548AComponent *parent_;
};

class TCA9548AComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void update();

  void set_disable_channels_after_io(bool disable) { this->disable_channels_after_io_ = disable; }

  i2c::ErrorCode switch_to_channel(uint8_t channel);
  void disable_all_channels();

 protected:
  friend class TCA9548AChannel;
  uint8_t current_channel_{255};
  bool disable_channels_after_io_{};  // Disable channels after any IO; use when multiple tca9548a are used
};
}  // namespace tca9548a
}  // namespace esphome
