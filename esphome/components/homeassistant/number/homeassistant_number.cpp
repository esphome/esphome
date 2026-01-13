#include "homeassistant_number.h"

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.number";

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
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, nullptr, std::bind(&HomeassistantNumber::state_changed_, this, std::placeholders::_1));

  api::global_api_server->get_home_assistant_state(
      this->entity_id_, "min", std::bind(&HomeassistantNumber::min_retrieved_, this, std::placeholders::_1));
  api::global_api_server->get_home_assistant_state(
      this->entity_id_, "max", std::bind(&HomeassistantNumber::max_retrieved_, this, std::placeholders::_1));
  api::global_api_server->get_home_assistant_state(
      this->entity_id_, "step", std::bind(&HomeassistantNumber::step_retrieved_, this, std::placeholders::_1));
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

  this->publish_state(value);

  static constexpr auto SERVICE_NAME = StringRef::from_lit("number.set_value");
  static constexpr auto ENTITY_ID_KEY = StringRef::from_lit("entity_id");
  static constexpr auto VALUE_KEY = StringRef::from_lit("value");

  api::HomeassistantActionRequest resp;
  resp.service = SERVICE_NAME;

  resp.data.init(2);
  auto &entity_id = resp.data.emplace_back();
  entity_id.key = ENTITY_ID_KEY;
  entity_id.value = StringRef(this->entity_id_);

  auto &entity_value = resp.data.emplace_back();
  entity_value.key = VALUE_KEY;
  // Stack buffer - no heap allocation; %g produces shortest representation
  char value_buf[16];
  snprintf(value_buf, sizeof(value_buf), "%g", value);
  entity_value.value = StringRef(value_buf);

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace homeassistant
}  // namespace esphome
