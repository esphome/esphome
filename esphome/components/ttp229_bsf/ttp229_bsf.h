#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <vector>

namespace esphome {
namespace ttp229_bsf {

class TTP229BSFChannel : public binary_sensor::BinarySensor {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }
  void process(uint16_t data) { this->publish_state(data & (1 << this->channel_)); }

 protected:
  uint8_t channel_;
};

class TTP229BSFComponent : public Component {
 public:
  void set_sdo_pin(GPIOPin *sdo_pin) { sdo_pin_ = sdo_pin; }
  void set_scl_pin(GPIOPin *scl_pin) { scl_pin_ = scl_pin; }
  void register_channel(TTP229BSFChannel *channel) { this->channels_.push_back(channel); }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override {
    // check datavalid if sdo is high
    if (!this->sdo_pin_->digital_read()) {
      return;
    }
    uint16_t touched = 0;
    for (uint8_t i = 0; i < 16; i++) {
      this->scl_pin_->digital_write(false);
      delayMicroseconds(2);  // 500KHz
      bool bitval = !this->sdo_pin_->digital_read();
      this->scl_pin_->digital_write(true);
      delayMicroseconds(2);  // 500KHz

      touched |= uint16_t(bitval) << i;
    }
    for (auto *channel : this->channels_) {
      channel->process(touched);
    }
  }

 protected:
  GPIOPin *sdo_pin_;
  GPIOPin *scl_pin_;
  std::vector<TTP229BSFChannel *> channels_{};
};

}  // namespace ttp229_bsf
}  // namespace esphome
