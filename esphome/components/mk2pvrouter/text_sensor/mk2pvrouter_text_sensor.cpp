#include "mk2pvrouter_text_sensor.h"

namespace esphome::mk2pvrouter {

Mk2PVRouterTextSensor::Mk2PVRouterTextSensor(const char *tag) : Mk2PVRouterListener(tag) {}

void Mk2PVRouterTextSensor::publish_val(const char *val) { this->publish_state(val); }

}  // namespace esphome::mk2pvrouter
