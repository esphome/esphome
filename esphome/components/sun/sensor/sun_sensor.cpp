#include "sun_sensor.h"
#include "esphome/core/log.h"

namespace esphome::sun {

ESPHOME_LOG_TAG(TAG, "sun.sensor");

void SunSensor::dump_config() { LOG_SENSOR("", "Sun Sensor", this); }

}  // namespace esphome::sun
