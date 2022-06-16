#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace es8388 {

class ES8388Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
};

}  // namespace es8388
}  // namespace esphome
