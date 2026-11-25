#include "homeassistant.h"

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#ifdef USE_API_HOMEASSISTANT_SERVICES
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::homeassistant {

PROGMEM_STRING_TABLE(UnsetValues, "None", "unavailable", "unknown");

bool is_unset(StringRef value) { return progmem_string_table_find<UnsetValues>(value.c_str(), value.size()) >= 0; }

#ifdef USE_API_HOMEASSISTANT_SERVICES
bool can_send_action(const char *tag, const char *entity_id, [[maybe_unused]] EntityBase *entity) {
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(tag, "'%s': No clients connected to API server", entity_id);
    return false;
  }
#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (entity_id[0] == '\0') {
    ESP_LOGW(tag, "'%s': No entity ID set yet", LOG_STR_ARG(entity->get_log_name()));
    return false;
  }
#endif
  return true;
}
#endif

}  // namespace esphome::homeassistant
