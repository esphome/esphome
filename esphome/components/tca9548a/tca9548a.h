#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tca9548a {

static const uint8_t TCA9548A_DISABLE_CHANNELS_COMMAND = 0x00;

class TCA9548AComponent;
class TCA9548AChannel : public i2c::I2CBus {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }
  void set_parent(TCA9548AComponent *parent) { parent_ = parent; }

  i2c::ErrorCode readv(uint8_t address, i2c::ReadBuffer *buffers, size_t cnt) override;
  i2c::ErrorCode writev(uint8_t address, i2c::WriteBuffer *buffers, size_t cnt, bool stop) override;

  i2c::ErrorCode set_frequency(uint32_t frequency) override {
    this->frequency_ = frequency;
    return i2c::ERROR_OK;
  }
  uint32_t get_frequency() const override { return this->frequency_; }

 protected:
  uint8_t channel_;
  uint32_t frequency_;
  TCA9548AComponent *parent_;
};

class TCA9548AComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void update();

  i2c::ErrorCode switch_to_channel(uint8_t channel, uint32_t frequency);
  void disable_all_channels(bool restore_original_frequency);

 protected:
  friend class TCA9548AChannel;
  uint32_t original_frequency_;
};
}  // namespace tca9548a
}  // namespace esphome
