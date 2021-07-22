#include "sun_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sun {

static const char *const TAG = "sun.text_sensor";

void SunTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Sun Text Sensor", this); }

}  // namespace sun
}  // namespace esphome
