#include "homeassistant_button.h"

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.button");

void HomeassistantButton::dump_config() {
  LOG_BUTTON("", "Homeassistant Button", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_);
}

float HomeassistantButton::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void HomeassistantButton::press_action() {
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

  static constexpr auto SERVICE_BUTTON = StringRef::from_lit("button.press");
  static constexpr auto SERVICE_INPUT_BUTTON = StringRef::from_lit("input_button.press");
  static constexpr auto ENTITY_ID_KEY = StringRef::from_lit("entity_id");
  static constexpr char INPUT_PREFIX[] = "input_";

  api::HomeassistantActionRequest resp;
  if (strncmp(this->entity_id_, INPUT_PREFIX, sizeof(INPUT_PREFIX) - 1) == 0) {
    resp.service = SERVICE_INPUT_BUTTON;
  } else {
    resp.service = SERVICE_BUTTON;
  }

  resp.data.init(1);
  auto &entity_id_kv = resp.data.emplace_back();
  entity_id_kv.key = ENTITY_ID_KEY;
  entity_id_kv.value = StringRef(this->entity_id_);

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
