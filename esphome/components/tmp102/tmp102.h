#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

#include <cstdint>

namespace esphome::tmp102 {

class TMP102Component final : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
#ifdef USE_TMP102_CONFIGURE
  void setup() override;
  void set_configuration(uint16_t config, uint16_t high_limit, uint16_t low_limit) {
    this->config_ = config;
    this->high_limit_ = high_limit;
    this->low_limit_ = low_limit;
    this->configure_ = true;
  }
#endif
  void dump_config() override;
  void update() override;

 protected:
#ifdef USE_TMP102_CONFIGURE
  bool write_register_(uint8_t reg, uint16_t value);
  bool write_configuration_();
  bool check_configuration_();
  void read_temperature_();
  void update_configured_();

  uint16_t config_{0};
  uint16_t high_limit_{0};
  uint16_t low_limit_{0};
  bool configure_{false};
#endif
};

}  // namespace esphome::tmp102
