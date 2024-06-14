#include "HomeAssistantNumber.h"

namespace esphome {
namespace homeassistant_number {
static const char *const TAG = "homeassistant.number";

void HomeAssistantNumber::publish_api_state(float state) {
  ignore_api_updates_with_seconds(1);
  ESP_LOGD(TAG, "publish_api_state: '%s' %f", this->get_name().c_str(), state);
  const std::map<std::string, std::string> data = {
      {"entity_id", entity_id_.c_str()},
      {"value", std::to_string(state)},
  };
  call_homeassistant_service("number.set_value", data);
}

void HomeAssistantNumber::state_changed(std::string state) {
  ESP_LOGD(TAG, "'%s': state_changed %s", get_name().c_str(), state.c_str());
  auto number_value = parse_number<float>(state);
  if (can_update_from_api() && number_value.has_value()) {
    this->publish_state(number_value.value());
  }
}

void HomeAssistantNumber::min_changed(std::string min) {
  ESP_LOGD(TAG, "'%s': min_changed %s", get_name().c_str(), min.c_str());
  auto min_value = parse_number<float>(min);
  if (min_value.has_value()) {
    this->traits.set_min_value(min_value.value());
  }
}

void HomeAssistantNumber::max_changed(std::string max) {
  ESP_LOGD(TAG, "'%s': max_changed %s", get_name().c_str(), max.c_str());
  auto max_value = parse_number<float>(max);
  if (max_value.has_value()) {
    this->traits.set_max_value(max_value.value());
  }
}

void HomeAssistantNumber::step_changed(std::string step) {
  ESP_LOGD(TAG, "'%s': step_changed %s", get_name().c_str(), step.c_str());
  auto step_value = parse_number<float>(step);
  if (step_value.has_value()) {
    this->traits.set_step(step_value.value());
  }
}

void HomeAssistantNumber::setup() {
  ESP_LOGD(TAG, "'%s': Setup", get_name().c_str());
  subscribe_homeassistant_state(&HomeAssistantNumber::state_changed, this->entity_id_);
  subscribe_homeassistant_state(&HomeAssistantNumber::min_changed, this->entity_id_, "min");
  subscribe_homeassistant_state(&HomeAssistantNumber::max_changed, this->entity_id_, "max");
  subscribe_homeassistant_state(&HomeAssistantNumber::step_changed, this->entity_id_, "step");
}

void HomeAssistantNumber::control(float value) {
  ESP_LOGD(TAG, "'%s': control %f", get_name().c_str(), value);
  publish_state(value);
  publish_api_state(state);
  this->state_callback_.call(value);
}
}  // namespace homeassistant_number
}  // namespace esphome
