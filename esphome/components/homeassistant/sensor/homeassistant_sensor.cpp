#include "homeassistant_sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.sensor";

void HomeassistantSensor::setup() {
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, this->attribute_, [this](const std::string &state) {
        auto val = parse_number<float>(state);
        if (!val.has_value()) {
          ESP_LOGW(TAG, "'%s': Can't convert '%s' to number!", this->entity_id_.c_str(), state.c_str());
          this->publish_state(NAN);
          return;
        }

        if (this->attribute_.has_value()) {
          ESP_LOGD(TAG, "'%s::%s': Got attribute state %.2f", this->entity_id_.c_str(),
                   this->attribute_.value().c_str(), *val);
        } else {
          ESP_LOGD(TAG, "'%s': Got state %.2f", this->entity_id_.c_str(), *val);
        }
        this->publish_state(*val);
      });
}
void HomeassistantSensor::dump_config() {
  LOG_SENSOR("", "Homeassistant Sensor", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_.c_str());
  if (this->attribute_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Attribute: '%s'", this->attribute_.value().c_str());
  }
}
float HomeassistantSensor::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

}  // namespace homeassistant
}  // namespace esphome
