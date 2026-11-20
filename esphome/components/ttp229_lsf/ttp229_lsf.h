#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ttp229_lsf {

// A template on the sensor type keeps binary_sensor.h out of this hub header
template<typename T> inline void publish_channel(T *sensor, uint16_t touched, uint8_t channel) {
  sensor->publish_state((touched >> channel) & 1);
}

class TTP229LSFComponent final : public Component, public i2c::I2CDevice {
 public:
#ifdef USE_TTP229_LSF_CHANNELS_PUBLISHER
  HUB_PUBLISHER(channels, uint16_t)
#endif
  void setup() override;
  void dump_config() override;
  void loop() override;
};

}  // namespace esphome::ttp229_lsf
