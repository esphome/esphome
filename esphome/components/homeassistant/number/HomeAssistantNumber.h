#include <map>
#include <string>
#include "esphome/components/homeassistant/HomeAssistantComponent.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace homeassistant_number {
static const char* const TAG = "homeassistant.number";

class HomeAssistantNumber
    : public number::Number,
      public homeassistant::HomeAssistantComponent {

  void publish_api_state(float state) {
    ignore_api_updates_with_seconds(1);
    ESP_LOGD(TAG, "publish_api_state: '%s' %f", this->get_name().c_str(),
             state);
    const std::map<std::string, std::string> data = {
        {"entity_id", entity_id_.c_str()},
        {"value", std::to_string(state)},
    };
    call_homeassistant_service("number.set_value", data);
  }

  void state_changed(std::string state) {
    ESP_LOGD(TAG, "'%s': state_changed %s", get_name().c_str(), state.c_str());
    auto number_value = parse_number<float>(state);
    if (can_update_from_api() && number_value.has_value()) {
      this->publish_state(number_value.value());
    }
  }

  void setup() {
    ESP_LOGD(TAG, "'%s': Setup", get_name().c_str());
    subscribe_homeassistant_state(&HomeAssistantNumber::state_changed,
                                  this->entity_id_);
  }

  void control(float value) {
    ESP_LOGD(TAG, "'%s': control %f", get_name().c_str(), value);
    publish_state(value);
    publish_api_state(state);
    this->state_callback_.call(value);
  }
};
}  // namespace homeassistant_number
}  // namespace esphome
