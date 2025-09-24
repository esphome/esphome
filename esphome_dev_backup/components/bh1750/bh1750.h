#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bh1750 {

enum BH1750Mode {
  BH1750_MODE_L,
  BH1750_MODE_H,
  BH1750_MODE_H2,
};

/// This class implements support for the i2c-based BH1750 ambient light sensor.
class BH1750Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

 protected:
  void read_lx_(BH1750Mode mode, uint8_t mtreg, const std::function<void(float)> &f);

  uint8_t active_mtreg_{0};
};

}  // namespace bh1750
}  // namespace esphome
