#pragma once

#include "../systa_bus.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <span>

namespace esphome::systa_bus {

// Publishes the error code of an Aqua sensor data message: "--" for none, else two digits
void publish_aqua_error_code(text_sensor::TextSensor *sensor, std::span<const uint8_t> message);

}  // namespace esphome::systa_bus
