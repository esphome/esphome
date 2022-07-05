#include "mqtt_text_input.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_TEXT_INPUT

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.text_input";

using namespace esphome::text_input;

MQTTTextInputComponent::MQTTTextInputComponent(TextInput *text_input) : text_input_(text_input) {}


void MQTTTextInputComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto val = state.c_str();
    if (!val.has_value()) {
      ESP_LOGW(TAG, "Can't convert '%s' to text_input!", state.c_str());
      return;
    }
    auto call = this->text_input_->make_call();
    call.set_value(*val);
    call.perform();
  });

  this->text_input_->add_on_state_callback([this](std::string &state) { 
    this->publish_state(state); 
  });
}

void MQTTTextInputComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT text_input '%s':", this->text_input_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTTextInputComponent::component_type() const { return "text_input"; }
const EntityBase *MQTTTextInputComponent::get_entity() const { return this->text_input_; }


void MQTTTextInputComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  const auto &traits = text_input_->traits;
  // https://www.home-assistant.io/integrations/text_input.mqtt/
  switch (this->text_input_->traits.get_mode()) {
    case TEXT_INPUT_MODE_AUTO:
      break;
    case TEXT_INPUT_MODE_STRING:
      root[MQTT_MODE] = "string";
      break;
//    case TEXT_INPUT_MODE_SECRET:
//      root[MQTT_MODE] = "secret";
//      break;
  }

  config.command_topic = true;
}
bool MQTTTextInputComponent::send_initial_state() {
  if (this->text_input_->has_state()) {
    return this->publish_state(this->text_input_->state);
  } else {
    return true;
  }
}
bool MQTTTextInputComponent::publish_state(const std::string &value) { 
  return this->publish(this->get_state_topic_(), value); 
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
