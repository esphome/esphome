#include "homeassistant_text.h"

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

static const char *const TAG = "homeassistant.text";

void HomeassistantText::state_changed_(StringRef state) {
  if (state == this->state) {
    return;
  }
  ESP_LOGD(TAG, "'%s': Got state %s", this->entity_id_, state.c_str());
  this->publish_state(state.c_str(), state.size());
}

void HomeassistantText::min_retrieved_(StringRef min) {
  auto min_value = parse_number<int>(min.c_str());
  if (!min_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'min' value '%s' to number!", this->entity_id_, min.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Min retrieved: %s", this->entity_id_, min.c_str());
  this->traits.set_min_length(min_value.value());
}

void HomeassistantText::max_retrieved_(StringRef max) {
  auto max_value = parse_number<int>(max.c_str());
  if (!max_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'max' value '%s' to number!", this->entity_id_, max.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Max retrieved: %s", this->entity_id_, max.c_str());
  this->traits.set_max_length(max_value.value());
}

void HomeassistantText::mode_retrieved_(StringRef mode) {
  if (mode == "text") {
    this->traits.set_mode(text::TEXT_MODE_TEXT);
  } else if (mode == "password") {
    this->traits.set_mode(text::TEXT_MODE_PASSWORD);
  } else {
    ESP_LOGW(TAG, "'%s': Unknown 'mode' value '%s'", this->entity_id_, mode.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Mode retrieved: %s", this->entity_id_, mode.c_str());
}

void HomeassistantText::setup() {
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr,
                                                         [this](StringRef state) { this->state_changed_(state); });

  api::global_api_server->get_home_assistant_state(this->entity_id_, "min",
                                                   [this](StringRef min) { this->min_retrieved_(min); });
  api::global_api_server->get_home_assistant_state(this->entity_id_, "max",
                                                   [this](StringRef max) { this->max_retrieved_(max); });
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
    ESP_LOGW(TAG, "'%s': No entity ID set yet", this->get_name().c_str());
    return;
  }
#endif

  static constexpr auto SERVICE_TEXT = StringRef::from_lit("text.set_value");
  static constexpr auto SERVICE_INPUT_TEXT = StringRef::from_lit("input_text.set_value");
  static constexpr auto ENTITY_ID_KEY = StringRef::from_lit("entity_id");
  static constexpr auto VALUE_KEY = StringRef::from_lit("value");
  static constexpr char INPUT_PREFIX[] = "input_";

  api::HomeassistantActionRequest resp;
  if (strncmp(this->entity_id_, INPUT_PREFIX, sizeof(INPUT_PREFIX) - 1) == 0) {
    resp.service = SERVICE_INPUT_TEXT;
  } else {
    resp.service = SERVICE_TEXT;
  }

  resp.data.init(2);
  auto &entity_id_kv = resp.data.emplace_back();
  entity_id_kv.key = ENTITY_ID_KEY;
  entity_id_kv.value = StringRef(this->entity_id_);

  auto &value_kv = resp.data.emplace_back();
  value_kv.key = VALUE_KEY;
  value_kv.value = StringRef(value.data(), value.size());

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
