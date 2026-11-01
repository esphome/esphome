#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::mk2pvrouter {

// Publishes whether `val` is not "0" when `tag` is `own_tag`
void publish_binary_sensor_value(binary_sensor::BinarySensor *sensor, const char *own_tag, const char *tag,
                                 const char *val);
void dump_binary_sensor(const char *tag, binary_sensor::BinarySensor *sensor, const char *own_tag);

}  // namespace esphome::mk2pvrouter
