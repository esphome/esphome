#include "api_wizard.h"

#ifdef USE_API_WIZARD

#include <cstring>

#include "api_connection.h"
#include "api_pb2.h"
#include "api_server.h"
#include "esphome/core/log.h"

namespace esphome::api {

ESPHOME_LOG_TAG(TAG, "api.wizard");

#ifdef USE_API_WIZARD_INPUTS
#ifdef USE_API_WIZARD_MULTIPLE_INPUTS
// Walks the comma separated entity ids in value, without the spaces around each. Returns the length they take joined
// by ',', or 0 when one is empty or has no '.'. Writes the joined list to out unless it is nullptr.
static size_t wizard_join_entity_ids(const char *value, size_t length, char *out) {
  size_t joined = 0;
  const char *end = value + length;
  while (true) {
    const char *comma = static_cast<const char *>(memchr(value, ',', end - value));
    const char *id_end = comma != nullptr ? comma : end;
    while (value < id_end && *value == ' ')
      value++;
    while (id_end > value && id_end[-1] == ' ')
      id_end--;
    size_t id_length = id_end - value;
    if (id_length == 0 || memchr(value, '.', id_length) == nullptr)
      return 0;
    if (joined != 0) {
      if (out != nullptr)
        out[joined] = ',';
      joined++;
    }
    if (out != nullptr)
      memcpy(out + joined, value, id_length);
    joined += id_length;
    if (comma == nullptr)
      return joined;
    value = comma + 1;
  }
}
#endif  // USE_API_WIZARD_MULTIPLE_INPUTS

static bool wizard_entity_id_valid(const char *entity_id, size_t length) {
  return length > 0 && length < WIZARD_ENTITY_ID_BUFFER_SIZE && memchr(entity_id, '.', length) != nullptr;
}

const char *wizard_set_input(const WizardInputSetRequest &msg) {
  const char *value = msg.entity_id.c_str();
  const size_t length = msg.entity_id.size();
  for (size_t i = 0; i < API_WIZARD_INPUT_COUNT; i++) {
    // The table is in flash, which ESP8266 can only read through progmem_memcpy
    WizardInputEntry entry;
    progmem_memcpy(&entry, &API_WIZARD_INPUTS[i], sizeof(entry));
    if (entry.key != msg.key)
      continue;
#ifdef USE_API_WIZARD_MULTIPLE_INPUTS
    if (entry.size != 0) {
      // Two passes, validate and measure then write, so a rejected request leaves the buffer unchanged
      const size_t joined = wizard_join_entity_ids(value, length, nullptr);
      if (joined == 0 || joined >= entry.size)
        break;
      wizard_join_entity_ids(value, length, entry.entity_id);
      entry.entity_id[joined] = '\0';
      return entry.entity_id;
    }
#endif
    if (!wizard_entity_id_valid(value, length))
      break;
    memcpy(entry.entity_id, value, length);
    entry.entity_id[length] = '\0';
    return entry.entity_id;
  }
  ESP_LOGW(TAG, "Ignoring an entity id for an unknown wizard input, or one that is not valid");
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
