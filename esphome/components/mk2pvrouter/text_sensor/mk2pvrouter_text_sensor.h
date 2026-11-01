#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::mk2pvrouter {

// Publishes `val` verbatim when `tag` is `own_tag`
void publish_text_sensor_value(text_sensor::TextSensor *sensor, const char *own_tag, const char *tag, const char *val);
void dump_text_sensor(text_sensor::TextSensor *sensor, const char *own_tag);

}  // namespace esphome::mk2pvrouter
