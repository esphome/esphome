#include "sun_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sun {

static const char *const TAG = "sun.sensor";

void SunSensor::dump_config() { LOG_SENSOR("", "Sun Sensor", this); }

}  // namespace sun
}  // namespace esphome
