#include "api_wizard.h"

#ifdef USE_API_WIZARD

#include <cstring>

#include "api_connection.h"
#include "api_pb2.h"
#include "api_server.h"
#include "esphome/core/log.h"

namespace esphome::api {

static const char *const TAG = "api.wizard";

#ifdef USE_API_WIZARD_INPUTS
static bool wizard_entity_id_valid(const char *entity_id, size_t length) {
  return length > 0 && length < WIZARD_ENTITY_ID_BUFFER_SIZE && memchr(entity_id, '.', length) != nullptr;
}

const char *wizard_set_input(const WizardInputSetRequest &msg) {
  if (!wizard_entity_id_valid(msg.entity_id.c_str(), msg.entity_id.size())) {
    ESP_LOGW(TAG, "Ignoring an invalid entity id for wizard input");
    return nullptr;
  }
  for (size_t i = 0; i < API_WIZARD_INPUT_COUNT; i++) {
    // The table is in flash, which ESP8266 can only read through progmem_memcpy
    WizardInputEntry entry;
    progmem_memcpy(&entry, &API_WIZARD_INPUTS[i], sizeof(entry));
    if (entry.key != msg.key)
      continue;
    memcpy(entry.entity_id, msg.entity_id.c_str(), msg.entity_id.size());
    entry.entity_id[msg.entity_id.size()] = '\0';
    return entry.entity_id;
  }
  ESP_LOGW(TAG, "Ignoring an entity id for an unknown wizard input");
  return nullptr;
}
#endif  // USE_API_WIZARD_INPUTS

void APIConnection::on_device_wizard_request() {
  DeviceWizardResponse resp;
  resp.data = API_WIZARD_DATA;
  resp.data_len = API_WIZARD_DATA_SIZE;
  if (!this->send_message(resp)) {
    this->on_fatal_error();
  }
}

#ifdef USE_API_WIZARD_INPUTS
void APIConnection::on_wizard_input_set_request(const WizardInputSetRequest &msg) {
  const char *entity_id = wizard_set_input(msg);
  if (entity_id == nullptr)
    return;
#ifdef USE_API_WIZARD_LINKED_INPUTS
  // Entities subscribed to the buffer before it held an entity id, so every client needs to learn of it now
  for (auto &client : this->parent_->active_clients()) {
    client->resend_state_subscriptions(entity_id);
  }
#endif
}
#endif

#ifdef USE_API_WIZARD_LINKED_INPUTS
void APIConnection::resend_state_subscriptions(const char *entity_id) {
  if (!this->flags_.home_assistant_states)
    return;
  for (const auto &it : this->parent_->get_state_subs()) {
    if (it.entity_id != entity_id)
      continue;
    SubscribeHomeAssistantStateResponse resp;
    resp.entity_id = StringRef(it.entity_id);
    resp.attribute = it.attribute != nullptr ? StringRef(it.attribute) : StringRef("");
    resp.once = it.once;
    if (!this->send_message(resp)) {
      // Could not send now: send every subscription again from the loop
      this->state_subs_at_ = 0;
      return;
    }
  }
}
#endif

}  // namespace esphome::api

#endif  // USE_API_WIZARD
