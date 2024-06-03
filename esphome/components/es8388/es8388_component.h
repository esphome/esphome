#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace es8388 {

class ES8388Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void setup_raspiaudio_radio();
  void setup_raspiaudio_muse_luxe();

  float get_setup_priority() const override { return setup_priority::LATE - 1; }

  void powerup_dac();
  // void powerup_adc();
  void powerup();

  void powerdown_dac();
  void powerdown_adc();
  void powerdown();

  void clock_mode(uint8_t mode);

  void mute();
};

}  // namespace es8388
}  // namespace esphome
