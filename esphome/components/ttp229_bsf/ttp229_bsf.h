#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::ttp229_bsf {

// A template on the sensor type keeps binary_sensor.h out of this hub header
template<typename T> inline void publish_channel(T *sensor, uint16_t touched, uint8_t channel) {
  sensor->publish_state((touched >> channel) & 1);
}

class TTP229BSFComponent final : public Component {
 public:
  void set_sdo_pin(GPIOPin *sdo_pin) { sdo_pin_ = sdo_pin; }
  void set_scl_pin(GPIOPin *scl_pin) { scl_pin_ = scl_pin; }
#ifdef USE_TTP229_BSF_CHANNELS_PUBLISHER
  HUB_PUBLISHER(channels, uint16_t)
#endif
  void setup() override;
  void dump_config() override;
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
#ifdef USE_TTP229_BSF_CHANNELS_PUBLISHER
    if (this->channels_publisher_ != nullptr)
      this->channels_publisher_(touched);
#endif
  }

 protected:
  GPIOPin *sdo_pin_;
  GPIOPin *scl_pin_;
};

}  // namespace esphome::ttp229_bsf
