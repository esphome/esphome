#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace es8388 {

class ES8388Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;

  float get_setup_priority() const override { return setup_priority::LATE - 1; }
};

}  // namespace es8388
}  // namespace esphome
