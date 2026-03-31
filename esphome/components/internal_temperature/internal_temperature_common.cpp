#include "esphome/core/log.h"
#include "internal_temperature.h"

namespace esphome {
namespace internal_temperature {

static const char *const TAG = "internal_temperature";

void InternalTemperatureSensor::dump_config() { LOG_SENSOR("", "Internal Temperature Sensor", this); }

}  // namespace internal_temperature
}  // namespace esphome
