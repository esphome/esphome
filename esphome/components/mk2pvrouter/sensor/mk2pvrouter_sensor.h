#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::mk2pvrouter {

// Publishes `val` when `tag` is `own_tag`; with `scale_centi` the device sent the value x100
void publish_sensor_value(sensor::Sensor *sensor, const char *own_tag, bool scale_centi, const char *tag,
                          const char *val);
void dump_sensor(const char *log_tag, sensor::Sensor *sensor, const char *own_tag);

}  // namespace esphome::mk2pvrouter
