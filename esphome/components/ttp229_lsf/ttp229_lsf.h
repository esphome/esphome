#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <vector>

namespace esphome::ttp229_lsf {

class TTP229Channel final : public binary_sensor::BinarySensor {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }
  void process(uint16_t data) { this->publish_state(data & (1 << this->channel_)); }

 protected:
  uint8_t channel_;
};

class TTP229LSFComponent final : public Component, public i2c::I2CDevice {
 public:
#ifdef TTP229_LSF_CHANNEL_COUNT
  SLOT_LIST(TTP229Channel, channels_, register_channel, TTP229_LSF_CHANNEL_COUNT)
#else
  SLOT_LIST_EMPTY(TTP229Channel, channels_)
#endif
  void setup() override;
  void dump_config() override;
  void loop() override;
};

}  // namespace esphome::ttp229_lsf
