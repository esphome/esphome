#include "homeassistant_switch.h"
#include "../homeassistant.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.switch");

void HomeassistantSwitch::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr, [this](StringRef state) {
    auto val = parse_on_off(state.c_str());
    if (val != PARSE_ON && val != PARSE_OFF) {
      ESP_LOGW(TAG, "Can't convert '%s' to binary state!", state.c_str());
      return;
    }
    bool new_state = val == PARSE_ON;
    ESP_LOGD(TAG, "'%s': Got state %s", this->entity_id_, ONOFF(new_state));
    this->publish_state(new_state);
  });
}

void HomeassistantSwitch::dump_config() {
  LOG_SWITCH("", "Homeassistant Switch", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_);
}

float HomeassistantSwitch::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void HomeassistantSwitch::write_state(bool state) {
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "No clients connected to API server");
    return;
  }

#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (this->entity_id_[0] == '\0') {
    ESP_LOGW(TAG, "'%s': No entity ID set yet", LOG_STR_ARG(this->get_log_name()));
    return;
  }
#endif

  // In flash on the ESP8266 and encoded from there
  static constexpr char SERVICE_ON[] PROGMEM = "homeassistant.turn_on";
  static constexpr char SERVICE_OFF[] PROGMEM = "homeassistant.turn_off";

  api::HomeassistantActionRequest resp;
  resp.service = state ? ProgmemStringRef::from_lit(SERVICE_ON) : ProgmemStringRef::from_lit(SERVICE_OFF);

  resp.data.init(1);
  add_entity_id(resp, this->entity_id_);

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
