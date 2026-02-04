#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace i2c_device {

class I2CDeviceComponent : public Component, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
};

}  // namespace i2c_device
}  // namespace esphome
