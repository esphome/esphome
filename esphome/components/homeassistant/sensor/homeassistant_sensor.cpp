#include "homeassistant_sensor.h"
#include "../homeassistant.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.sensor");

// Kept out of the state callback so the path for a number stays as small as it was. Takes the terminated state
// rather than a StringRef, so the callback has only the pointer to keep across the parse.
static void __attribute__((noinline, cold)) publish_not_a_number(HomeassistantSensor *sensor, const char *entity_id,
                                                                 const char *state) {
  // Home Assistant sends these when there is no value, which is not an error
  if (is_unset(StringRef(state))) {
    ESP_LOGV(TAG, "'%s': No value (%s)", entity_id, state);
  } else {
    ESP_LOGW(TAG, "'%s': Can't convert '%s' to number!", entity_id, state);
  }
  sensor->publish_state(NAN);
}

void HomeassistantSensor::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, this->attribute_, [this](StringRef state) {
    auto val = parse_number<float>(state.c_str());
    if (!val.has_value()) {
      publish_not_a_number(this, this->entity_id_, state.c_str());
      return;
    }

    if (this->attribute_ != nullptr) {
      ESP_LOGV(TAG, "'%s::%s': Got attribute state %.2f", this->entity_id_, this->attribute_, *val);
    } else {
      ESP_LOGV(TAG, "'%s': Got state %.2f", this->entity_id_, *val);
    }
    this->publish_state(*val);
  });
}
void HomeassistantSensor::dump_config() {
  LOG_SENSOR("", "Homeassistant Sensor", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_);
  if (this->attribute_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Attribute: '%s'", this->attribute_);
  }
}
float HomeassistantSensor::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

}  // namespace esphome::homeassistant
