#include "homeassistant_button.h"

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.button");

void press_button_entity([[maybe_unused]] button::Button *button, const char *entity_id) {
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "No clients connected to API server");
    return;
  }

#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (entity_id[0] == '\0') {
    ESP_LOGW(TAG, "'%s': No entity ID set yet", LOG_STR_ARG(button->get_log_name()));
    return;
  }
#endif

  // In flash on the ESP8266 and encoded from there
  static const char SERVICE_BUTTON[] PROGMEM = "button.press";
  static const char SERVICE_INPUT_BUTTON[] PROGMEM = "input_button.press";
  static const char ENTITY_ID_KEY[] PROGMEM = "entity_id";

  api::HomeassistantActionRequest resp;
  // An input helper entity id starts with the "input_" its action name starts with
  const bool input_helper = ESPHOME_strncmp_P(entity_id, SERVICE_INPUT_BUTTON, sizeof("input_") - 1) == 0;
  resp.service =
      input_helper ? ProgmemStringRef::from_lit(SERVICE_INPUT_BUTTON) : ProgmemStringRef::from_lit(SERVICE_BUTTON);

  resp.data.init(1);
  auto &entity_id_kv = resp.data.emplace_back();
  entity_id_kv.key = ProgmemStringRef::from_lit(ENTITY_ID_KEY);
  entity_id_kv.value = ProgmemStringRef(StringRef(entity_id));

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
