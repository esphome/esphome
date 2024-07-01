#include "HomeAssistantNumber.h"

namespace esphome {
namespace homeassistant_number {
static const char *const TAG = "homeassistant.number";

void HomeAssistantNumber::publish_api_state_(float state) {
  ignore_api_updates_with_seconds_(1);
  ESP_LOGD(TAG, "publish_api_state_: '%s' %f", this->get_name().c_str(), state);
  const std::map<std::string, std::string> data = {
      {"entity_id", entity_id_.c_str()},
      {"value", std::to_string(state)},
  };
  call_homeassistant_service("number.set_value", data);
}

void HomeAssistantNumber::state_changed_(const std::string &state) {
  ESP_LOGD(TAG, "'%s': state_changed_ %s", get_name().c_str(), state.c_str());
  auto number_value = parse_number<float>(state);
  if (can_update_from_api_() && number_value.has_value()) {
    this->publish_state(number_value.value());
  }
}

void HomeAssistantNumber::min_changed_(const std::string &min) {
  ESP_LOGD(TAG, "'%s': min_changed %s", get_name().c_str(), min.c_str());
  auto min_value = parse_number<float>(min);
  if (min_value.has_value()) {
    this->traits.set_min_value(min_value.value());
  }
}

void HomeAssistantNumber::max_changed_(const std::string &max) {
  ESP_LOGD(TAG, "'%s': max_changed %s", get_name().c_str(), max.c_str());
  auto max_value = parse_number<float>(max);
  if (max_value.has_value()) {
    this->traits.set_max_value(max_value.value());
  }
}

void HomeAssistantNumber::step_changed_(const std::string &step) {
  ESP_LOGD(TAG, "'%s': step_changed %s", get_name().c_str(), step.c_str());
  auto step_value = parse_number<float>(step);
  if (step_value.has_value()) {
    this->traits.set_step(step_value.value());
  }
}

void HomeAssistantNumber::setup() {
  ESP_LOGD(TAG, "'%s': Setup", get_name().c_str());
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, optional<std::string>(""),
      std::bind(&HomeAssistantNumber::state_changed_, this, std::placeholders::_1));
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, optional<std::string>("min"),
      std::bind(&HomeAssistantNumber::min_changed_, this, std::placeholders::_1));
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, optional<std::string>("max"),
      std::bind(&HomeAssistantNumber::max_changed_, this, std::placeholders::_1));
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, optional<std::string>("step"),
      std::bind(&HomeAssistantNumber::step_changed_, this, std::placeholders::_1));
}

void HomeAssistantNumber::control(float value) {
  ESP_LOGD(TAG, "'%s': control %f", get_name().c_str(), value);
  publish_state(value);
  publish_api_state_(state);
  this->state_callback_.call(value);
}
}  // namespace homeassistant_number
}  // namespace esphome
