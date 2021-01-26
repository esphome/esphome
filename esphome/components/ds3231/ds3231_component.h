#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ds3231 {

class DS3231Component : public Component, public i2c::I2CDevice {
 public:
};

}  // namespace ds3231
}  // namespace esphome
