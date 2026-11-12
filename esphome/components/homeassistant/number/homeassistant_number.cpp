#include "homeassistant_number.h"

#include "../homeassistant.h"

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.number");

void HomeassistantNumber::state_changed_(StringRef state) {
  auto number_value = parse_number<float>(state.c_str());
  if (!number_value.has_value()) {
    ESP_LOGW(TAG, "'%s': Can't convert '%s' to number!", this->entity_id_, state.c_str());
    this->publish_state(NAN);
    return;
  }
  if (this->state == number_value.value()) {
    return;
  }
  ESP_LOGD(TAG, "'%s': Got state %s", this->entity_id_, state.c_str());
  this->publish_state(number_value.value());
}

optional<float> HomeassistantNumber::parse_attribute_(const char *name, StringRef value) {
  auto number = parse_number<float>(value.c_str());
  if (!number.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert '%s' value '%s' to number!", this->entity_id_, name, value.c_str());
    return nullopt;
  }
  ESP_LOGD(TAG, "'%s': Retrieved '%s': %s", LOG_STR_ARG(this->get_log_name()), name, value.c_str());
  return number;
}

void HomeassistantNumber::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr,
                                                         [this](StringRef state) { this->state_changed_(state); });

  api::global_api_server->get_home_assistant_state(this->entity_id_, "min", [this](StringRef value) {
    if (auto min = this->parse_attribute_("min", value))
      this->traits.set_min_value(*min);
  });
  api::global_api_server->get_home_assistant_state(this->entity_id_, "max", [this](StringRef value) {
    if (auto max = this->parse_attribute_("max", value))
      this->traits.set_max_value(*max);
  });
  api::global_api_server->get_home_assistant_state(this->entity_id_, "step", [this](StringRef value) {
    if (auto step = this->parse_attribute_("step", value))
      this->traits.set_step(*step);
  });
}

void HomeassistantNumber::dump_config() {
  LOG_NUMBER("", "Homeassistant Number", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_);
}

float HomeassistantNumber::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void HomeassistantNumber::control(float value) {
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

  this->publish_state(value);

  // In flash on the ESP8266 and encoded from there
  static constexpr char NUMBER_SERVICE_NAME[] PROGMEM = "number.set_value";
  static constexpr char INPUT_NUMBER_SERVICE_NAME[] PROGMEM = "input_number.set_value";
  static constexpr char VALUE_KEY[] PROGMEM = "value";

  api::HomeassistantActionRequest resp;
  resp.service = is_input_helper(this->entity_id_, INPUT_NUMBER_SERVICE_NAME)
                     ? ProgmemStringRef::from_lit(INPUT_NUMBER_SERVICE_NAME)
                     : ProgmemStringRef::from_lit(NUMBER_SERVICE_NAME);

  resp.data.init(2);
  add_entity_id(resp, this->entity_id_);

  auto &entity_value = resp.data.emplace_back();
  entity_value.key = ProgmemStringRef::from_lit(VALUE_KEY);
  // Stack buffer - no heap allocation; %g produces shortest representation
  char value_buf[16];
  buf_append_printf(value_buf, sizeof(value_buf), 0, "%g", value);
  entity_value.value = ProgmemStringRef(StringRef(value_buf));

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
