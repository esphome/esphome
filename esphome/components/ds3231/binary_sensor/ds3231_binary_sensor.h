#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "../ds3231.h"

namespace esphome::ds3231 {

class DS3231BinarySensor final : public binary_sensor::BinarySensor, public Parented<DS3231Component> {};

}  // namespace esphome::ds3231
