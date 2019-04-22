#include "homeassistant_sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

static const char *TAG = "homeassistant.sensor";

void HomeassistantSensor::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, [this](std::string state) {
    auto val = parse_float(state);
    if (!val.has_value()) {
      ESP_LOGW(TAG, "Can't convert '%s' to number!", state.c_str());
      this->publish_state(NAN);
      return;
    }

    ESP_LOGD(TAG, "'%s': Got state %.2f", this->entity_id_.c_str(), *val);
    this->publish_state(*val);
  });
}
void HomeassistantSensor::dump_config() {
  LOG_SENSOR("", "Homeassistant Sensor", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_.c_str());
}
float HomeassistantSensor::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

}  // namespace homeassistant
}  // namespace esphome
