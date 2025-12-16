#include "homeassistant_switch.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.switch";

using namespace esphome::switch_;

void HomeassistantSwitch::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr, [this](const std::string &state) {
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

  static constexpr auto SERVICE_ON = StringRef::from_lit("homeassistant.turn_on");
  static constexpr auto SERVICE_OFF = StringRef::from_lit("homeassistant.turn_off");
  static constexpr auto ENTITY_ID_KEY = StringRef::from_lit("entity_id");

  api::HomeassistantActionRequest resp;
  if (state) {
    resp.set_service(SERVICE_ON);
  } else {
    resp.set_service(SERVICE_OFF);
  }

  resp.data.init(1);
  auto &entity_id_kv = resp.data.emplace_back();
  entity_id_kv.set_key(ENTITY_ID_KEY);
  entity_id_kv.value = this->entity_id_;

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace homeassistant
}  // namespace esphome
