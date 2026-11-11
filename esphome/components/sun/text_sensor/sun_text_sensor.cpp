#include "sun_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome::sun {

ESPHOME_LOG_TAG(TAG, "sun.text_sensor");

void SunTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Sun Text Sensor", this); }

}  // namespace esphome::sun
