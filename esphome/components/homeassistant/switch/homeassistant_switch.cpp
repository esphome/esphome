#include "homeassistant_switch.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.switch");

// In flash on the ESP8266; copied to the stack only while a request is built
PROGMEM_STRING_TABLE(SwitchActionStrings, "homeassistant.turn_on", "homeassistant.turn_off", "entity_id");
enum SwitchActionString : uint8_t {
  SWITCH_ACTION_STRING_TURN_ON,
  SWITCH_ACTION_STRING_TURN_OFF,
  SWITCH_ACTION_STRING_ENTITY_ID,
};

using namespace esphome::switch_;

void HomeassistantSwitch::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr, [this](StringRef state) {
    auto val = parse_on_off(state.c_str());
    switch (val) {
      case PARSE_NONE:
      case PARSE_TOGGLE:
        ESP_LOGW(TAG, "Can't convert '%s' to binary state!", state.c_str());
        break;
      case PARSE_ON:
      case PARSE_OFF:
        bool new_state = val == PARSE_ON;
        ESP_LOGD(TAG, "'%s': Got state %s", this->entity_id_, ONOFF(new_state));
        this->publish_state(new_state);
        break;
    }
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
    ESP_LOGW(TAG, "'%s': No entity ID set yet", this->get_name().c_str());
    return;
  }
#endif

  ProgmemStringTableCopy<SwitchActionStrings> strings;
  api::HomeassistantActionRequest resp;
  resp.service = StringRef(strings.get(state ? SWITCH_ACTION_STRING_TURN_ON : SWITCH_ACTION_STRING_TURN_OFF));

  resp.data.init(1);
  auto &entity_id_kv = resp.data.emplace_back();
  entity_id_kv.key = StringRef(strings.get(SWITCH_ACTION_STRING_ENTITY_ID));
  entity_id_kv.value = StringRef(this->entity_id_);

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
