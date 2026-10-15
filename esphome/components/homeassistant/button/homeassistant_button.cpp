#include "homeassistant_button.h"

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.button");

// In flash on the ESP8266; copied to the stack only while a request is built
PROGMEM_STRING_TABLE(ButtonActionStrings, "button.press", "input_button.press", "entity_id");
enum ButtonActionString : uint8_t {
  BUTTON_ACTION_STRING_SERVICE,
  BUTTON_ACTION_STRING_INPUT_SERVICE,
  BUTTON_ACTION_STRING_ENTITY_ID,
};

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

  ProgmemStringTableCopy<ButtonActionStrings> strings;
  // An input helper entity id starts with the "input_" its action name starts with
  const bool input_helper =
      strncmp(this->entity_id_, strings.get(BUTTON_ACTION_STRING_INPUT_SERVICE), sizeof("input_") - 1) == 0;
  api::HomeassistantActionRequest resp;
  resp.service =
      StringRef(strings.get(input_helper ? BUTTON_ACTION_STRING_INPUT_SERVICE : BUTTON_ACTION_STRING_SERVICE));

  resp.data.init(1);
  auto &entity_id_kv = resp.data.emplace_back();
  entity_id_kv.key = StringRef(strings.get(BUTTON_ACTION_STRING_ENTITY_ID));
  entity_id_kv.value = StringRef(this->entity_id_);

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
