#pragma once

#include "../systa_bus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"

#include <span>

namespace esphome::systa_bus {

// Big-endian int16 in 0.1 degrees at `start` of an Aqua sensor data message
inline float aqua_temperature(std::span<const uint8_t> message, size_t start) {
  return encode_value<int16_t>(&message[start]) * 0.1f;
}

}  // namespace esphome::systa_bus
