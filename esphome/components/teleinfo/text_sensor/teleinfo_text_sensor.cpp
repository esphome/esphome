#include "esphome/core/log.h"
#include "teleinfo_text_sensor.h"
namespace esphome {
namespace teleinfo {

static const char *TAG = "teleinfo_text_sensor";
TeleInfoTextSensor::TeleInfoTextSensor(const char *tag) { this->tag = std::string(tag); }
void TeleInfoTextSensor::publish_val(std::string val) { publish_state(val); }
void TeleInfoTextSensor::dump_config() { LOG_TEXT_SENSOR("  ", tag.c_str(), this); }
}  // namespace teleinfo
}  // namespace esphome
