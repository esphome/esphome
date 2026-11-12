#include "homeassistant_text.h"

#include "../homeassistant.h"

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.text");

// Indexed by text::TextMode
PROGMEM_STRING_TABLE(TextModeNames, "text", "password");

void HomeassistantText::state_changed_(StringRef state) {
  if (state == this->state) {
    return;
  }
  ESP_LOGD(TAG, "'%s': Got state %s", this->entity_id_, state.c_str());
  this->publish_state(state.c_str(), state.size());
}

optional<int> HomeassistantText::parse_length_(const char *name, StringRef value) {
  auto length = parse_number<int>(value.c_str());
  if (!length.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert '%s' value '%s' to number!", this->entity_id_, name, value.c_str());
    return nullopt;
  }
  ESP_LOGD(TAG, "'%s': Retrieved '%s': %s", this->entity_id_, name, value.c_str());
  return length;
}

void HomeassistantText::mode_retrieved_(StringRef mode) {
  const int16_t index = progmem_string_table_find<TextModeNames>(mode.c_str(), mode.size());
  if (index < 0) {
    ESP_LOGW(TAG, "'%s': Unknown 'mode' value '%s'", this->entity_id_, mode.c_str());
    return;
  }
  this->traits.set_mode(static_cast<text::TextMode>(index));
  ESP_LOGD(TAG, "'%s': Mode retrieved: %s", this->entity_id_, mode.c_str());
}

void HomeassistantText::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr,
                                                         [this](StringRef state) { this->state_changed_(state); });

  api::global_api_server->get_home_assistant_state(this->entity_id_, "min", [this](StringRef value) {
    if (auto min = this->parse_length_("min", value))
      this->traits.set_min_length(*min);
  });
  api::global_api_server->get_home_assistant_state(this->entity_id_, "max", [this](StringRef value) {
    if (auto max = this->parse_length_("max", value))
      this->traits.set_max_length(*max);
  });
  api::global_api_server->get_home_assistant_state(this->entity_id_, "mode",
                                                   [this](StringRef mode) { this->mode_retrieved_(mode); });
  // The "pattern" attribute is not fetched: Home Assistant sends "None" when it is unset,
  // and a runtime pattern would need its own buffer.
}

void HomeassistantText::dump_config() {
  LOG_TEXT("", "Homeassistant Text", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_);
}

float HomeassistantText::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void HomeassistantText::control(const std::string &value) {
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
  static constexpr char SERVICE_TEXT[] PROGMEM = "text.set_value";
  static constexpr char SERVICE_INPUT_TEXT[] PROGMEM = "input_text.set_value";
  static constexpr char VALUE_KEY[] PROGMEM = "value";

  api::HomeassistantActionRequest resp;
  resp.service = is_input_helper(this->entity_id_, SERVICE_INPUT_TEXT) ? ProgmemStringRef::from_lit(SERVICE_INPUT_TEXT)
                                                                       : ProgmemStringRef::from_lit(SERVICE_TEXT);

  resp.data.init(2);
  add_entity_id(resp, this->entity_id_);

  auto &value_kv = resp.data.emplace_back();
  value_kv.key = ProgmemStringRef::from_lit(VALUE_KEY);
  value_kv.value = ProgmemStringRef(value.data(), value.size());

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
