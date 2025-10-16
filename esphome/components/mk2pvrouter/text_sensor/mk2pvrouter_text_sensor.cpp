#include "esphome/core/log.h"
#include "mk2pvrouter_text_sensor.h"

namespace esphome {
namespace mk2pvrouter {
static const char *const TAG = "mk2pvrouter_text_sensor";

Mk2PVRouterTextSensor::Mk2PVRouterTextSensor(const char *tag) { this->tag = std::string(tag); }

void Mk2PVRouterTextSensor::publish_val(const std::string &val) { publish_state(val); }

void Mk2PVRouterTextSensor::dump_config() { LOG_TEXT_SENSOR("  ", "Mk2PVRouter Text Sensor", this); }
}  // namespace mk2pvrouter
}  // namespace esphome
