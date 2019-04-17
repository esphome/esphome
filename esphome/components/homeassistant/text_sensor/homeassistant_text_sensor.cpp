#include "homeassistant_text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

static const char *TAG = "homeassistant.text_sensor";

void HomeassistantTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Homeassistant Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_.c_str());
}
void HomeassistantTextSensor::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, [this](std::string state) {
    ESP_LOGD(TAG, "'%s': Got state '%s'", this->entity_id_.c_str(), state.c_str());
    this->publish_state(state);
  });
}

}  // namespace homeassistant
}  // namespace esphome
