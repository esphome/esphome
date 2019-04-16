#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ttp229 {

class TTP229Channel : public binary_sensor::BinarySensor {
 public:
  TTP229Channel(const std::string &name, int channel) : BinarySensor(name), channel_(channel) {}
  void process(uint16_t data) { this->publish_state(data & (1 << this->channel_)); }

 protected:
  int channel_;
};

class TTP229LSFComponent : public Component, public i2c::I2CDevice {
 public:
  void register_channel(TTP229Channel *channel) { this->channels_.push_back(channel); }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;

 protected:
  std::vector<TTP229Channel *> channels_{};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
  } error_code_{NONE};
};

}  // namespace ttp229
}  // namespace esphome
