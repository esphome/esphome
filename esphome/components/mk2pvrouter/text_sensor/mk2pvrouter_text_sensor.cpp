#include "esphome/core/log.h"
#include "mk2pvrouter_text_sensor.h"

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter_text_sensor";

Mk2PVRouterTextSensor::Mk2PVRouterTextSensor(const char *tag) { this->set_tag(tag); }

void Mk2PVRouterTextSensor::publish_val(const char *val) { this->publish_state(val); }

void Mk2PVRouterTextSensor::dump_config() { LOG_TEXT_SENSOR("  ", "Mk2PVRouter Text Sensor", this); }

}  // namespace esphome::mk2pvrouter
