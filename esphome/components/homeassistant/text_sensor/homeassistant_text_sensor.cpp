#include "homeassistant_text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.text_sensor";

void HomeassistantTextSensor::setup() {
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, this->attribute_, [this](const std::string &state) {
        if (this->attribute_.has_value()) {
          ESP_LOGD(TAG, "'%s::%s': Got attribute state '%s'", this->entity_id_.c_str(),
                   this->attribute_.value().c_str(), state.c_str());
        } else {
          ESP_LOGD(TAG, "'%s': Got state '%s'", this->entity_id_.c_str(), state.c_str());
        }
        this->publish_state(state);
      });
}
void HomeassistantTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Homeassistant Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_.c_str());
  if (this->attribute_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Attribute: '%s'", this->attribute_.value().c_str());
  }
}
float HomeassistantTextSensor::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }
}  // namespace homeassistant
}  // namespace esphome
