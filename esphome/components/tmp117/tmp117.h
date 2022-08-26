#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tmp117 {

class TMP117Component : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void set_config(uint16_t config) { config_ = config; };

 protected:
  bool read_data_(const int16_t *data);
  bool read_config_(const uint16_t *config);
  bool write_config_(uint16_t config);

  uint16_t config_;
};

}  // namespace tmp117
}  // namespace esphome
