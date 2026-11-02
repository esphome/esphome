#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::am2320 {

class AM2320Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  bool read_data_(uint8_t *data);
  bool read_bytes_(uint8_t a_register, uint8_t *data, uint8_t len, uint32_t conversion = 0);
};

}  // namespace esphome::am2320
