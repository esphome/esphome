#include "esphome/core/log.h"
#include "teleinfo_text_sensor.h"
namespace esphome {
namespace teleinfo {

static const char *const TAG = "teleinfo_text_sensor";
TeleInfoTextSensor::TeleInfoTextSensor(const char *tag) { this->tag = std::string(tag); }
void TeleInfoTextSensor::publish_val(const std::string &val) { publish_state(val); }
void TeleInfoTextSensor::dump_config() { LOG_TEXT_SENSOR("  ", "Teleinfo Text Sensor", this); }
}  // namespace teleinfo
}  // namespace esphome
