#include "mqtt_text.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_TEXT

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.text";

using namespace esphome::text;

MQTTTextComponent::MQTTTextComponent(Text *text) : text_(text) {}

void MQTTTextComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto call = this->text_->make_call();
    call.set_value(state);
    call.perform();
  });

  this->text_->add_on_state_callback([this](const std::string &state) { this->publish_state(state); });
}

void MQTTTextComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT text '%s':", this->text_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true)
}

std::string MQTTTextComponent::component_type() const { return "text"; }
const EntityBase *MQTTTextComponent::get_entity() const { return this->text_; }

void MQTTTextComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  switch (this->text_->traits.get_mode()) {
    case TEXT_MODE_TEXT:
      root[MQTT_MODE] = "text";
      break;
    case TEXT_MODE_PASSWORD:
      root[MQTT_MODE] = "password";
      break;
  }

  config.command_topic = true;
}
bool MQTTTextComponent::send_initial_state() {
  if (this->text_->has_state()) {
    return this->publish_state(this->text_->state);
  } else {
    return true;
  }
}
bool MQTTTextComponent::publish_state(const std::string &value) {
  return this->publish(this->get_state_topic_(), value);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
