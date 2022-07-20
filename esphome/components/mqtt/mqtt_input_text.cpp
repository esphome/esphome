#include "mqtt_input_text.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_INPUT_TEXT

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.input_text";

using namespace esphome::input_text;

MQTTInputTextComponent::MQTTInputTextComponent(InputText *input_text) : input_text_(input_text) {}

void MQTTInputTextComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    //const auto *val = state.c_str();
    auto call = this->input_text_->make_call();
    call.set_value(state.c_str());
    //call.set_value(*val);
    call.perform();
  });

  this->input_text_->add_on_state_callback([this](std::string &state) { this->publish_state(state); });
}

void MQTTInputTextComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT input_text '%s':", this->input_text_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTInputTextComponent::component_type() const { return "input_text"; }
const EntityBase *MQTTInputTextComponent::get_entity() const { return this->input_text_; }

void MQTTInputTextComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  //const auto &traits = input_text_->traits;

  switch (this->input_text_->traits.get_mode()) {
    case INPUT_TEXT_MODE_AUTO:
      break;
    case INPUT_TEXT_MODE_STRING:
      root[MQTT_MODE] = "string";
      break;
    case INPUT_TEXT_MODE_PASSWORD:
      root[MQTT_MODE] = "password";
      break;
  }

  config.command_topic = true;
}
bool MQTTInputTextComponent::send_initial_state() {
  if (this->input_text_->has_state()) {
    return this->publish_state(this->input_text_->state);
  } else {
    return true;
  }
}
bool MQTTInputTextComponent::publish_state(const std::string &value) {
  return this->publish(this->get_state_topic_(), value);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
