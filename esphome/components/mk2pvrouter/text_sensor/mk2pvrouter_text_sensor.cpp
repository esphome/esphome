#include "mk2pvrouter_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter_text_sensor";

Mk2PVRouterTextSensor::Mk2PVRouterTextSensor(const char *tag) : Mk2PVRouterListener(tag) {}

void Mk2PVRouterTextSensor::publish_val(const char *val) { this->publish_state(val); }

void Mk2PVRouterTextSensor::dump_config() {
  LOG_TEXT_SENSOR("  ", "Mk2PVRouter Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Tag: %s", this->get_tag());
}

}  // namespace esphome::mk2pvrouter
