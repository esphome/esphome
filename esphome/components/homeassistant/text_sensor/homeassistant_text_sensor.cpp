#include "homeassistant_text_sensor.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.text_sensor";

void HomeassistantTextSensor::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, this->attribute_, [this](StringRef state) {
    if (this->attribute_ != nullptr) {
      ESP_LOGD(TAG, "'%s::%s': Got attribute state '%s'", this->entity_id_, this->attribute_, state.c_str());
    } else {
      ESP_LOGD(TAG, "'%s': Got state '%s'", this->entity_id_, state.c_str());
    }
    this->publish_state(state.c_str(), state.size());
  });
}
void HomeassistantTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Homeassistant Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_);
  if (this->attribute_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Attribute: '%s'", this->attribute_);
  }
}
float HomeassistantTextSensor::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }
}  // namespace homeassistant
}  // namespace esphome
