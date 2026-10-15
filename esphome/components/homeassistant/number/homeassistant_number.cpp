#include "homeassistant_number.h"

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.number");

// In flash on the ESP8266; copied to the stack only while a request is built
PROGMEM_STRING_TABLE(NumberActionStrings, "number.set_value", "input_number.set_value", "entity_id", "value");
enum NumberActionString : uint8_t {
  NUMBER_ACTION_STRING_SERVICE,
  NUMBER_ACTION_STRING_INPUT_SERVICE,
  NUMBER_ACTION_STRING_ENTITY_ID,
  NUMBER_ACTION_STRING_VALUE,
};

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

void HomeassistantNumber::min_retrieved_(StringRef min) {
  auto min_value = parse_number<float>(min.c_str());
  if (!min_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'min' value '%s' to number!", this->entity_id_, min.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Min retrieved: %s", get_name().c_str(), min.c_str());
  this->traits.set_min_value(min_value.value());
}

void HomeassistantNumber::max_retrieved_(StringRef max) {
  auto max_value = parse_number<float>(max.c_str());
  if (!max_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'max' value '%s' to number!", this->entity_id_, max.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Max retrieved: %s", get_name().c_str(), max.c_str());
  this->traits.set_max_value(max_value.value());
}

void HomeassistantNumber::step_retrieved_(StringRef step) {
  auto step_value = parse_number<float>(step.c_str());
  if (!step_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'step' value '%s' to number!", this->entity_id_, step.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Step Retrieved %s", get_name().c_str(), step.c_str());
  this->traits.set_step(step_value.value());
}

void HomeassistantNumber::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr,
                                                         [this](StringRef state) { this->state_changed_(state); });

  api::global_api_server->get_home_assistant_state(this->entity_id_, "min",
                                                   [this](StringRef min) { this->min_retrieved_(min); });
  api::global_api_server->get_home_assistant_state(this->entity_id_, "max",
                                                   [this](StringRef max) { this->max_retrieved_(max); });
  api::global_api_server->get_home_assistant_state(this->entity_id_, "step",
                                                   [this](StringRef step) { this->step_retrieved_(step); });
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
    ESP_LOGW(TAG, "'%s': No entity ID set yet", this->get_name().c_str());
    return;
  }
#endif

  this->publish_state(value);

  ProgmemStringTableCopy<NumberActionStrings> strings;
  // An input helper entity id starts with the "input_" its action name starts with
  const bool input_helper =
      strncmp(this->entity_id_, strings.get(NUMBER_ACTION_STRING_INPUT_SERVICE), sizeof("input_") - 1) == 0;
  api::HomeassistantActionRequest resp;
  // input_number entities have their own set_value action
  resp.service =
      StringRef(strings.get(input_helper ? NUMBER_ACTION_STRING_INPUT_SERVICE : NUMBER_ACTION_STRING_SERVICE));

  resp.data.init(2);
  auto &entity_id = resp.data.emplace_back();
  entity_id.key = StringRef(strings.get(NUMBER_ACTION_STRING_ENTITY_ID));
  entity_id.value = StringRef(this->entity_id_);

  auto &entity_value = resp.data.emplace_back();
  entity_value.key = StringRef(strings.get(NUMBER_ACTION_STRING_VALUE));
  // Stack buffer - no heap allocation; %g produces shortest representation
  char value_buf[16];
  buf_append_printf(value_buf, sizeof(value_buf), 0, "%g", value);
  entity_value.value = StringRef(value_buf);

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
