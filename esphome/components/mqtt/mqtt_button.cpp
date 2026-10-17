#include "mqtt_button.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_BUTTON

namespace esphome::mqtt {

ESPHOME_LOG_TAG(TAG, "mqtt.button");

using namespace esphome::button;

MQTTButtonComponent::MQTTButtonComponent(button::Button *button) : button_(button) {}

void MQTTButtonComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    if (payload == "PRESS") {
      this->button_->press();
    } else {
      ESP_LOGW(TAG, "'%s': Received unknown status payload: %s", LOG_STR_ARG(this->log_name_()), payload.c_str());
      this->status_momentary_warning(5000);
    }
  });
}
void MQTTButtonComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Button '%s': ", LOG_STR_ARG(this->button_->get_log_name()));
  LOG_MQTT_COMPONENT(false, true);
}

void MQTTButtonComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  config.state_topic = false;
}

MQTT_COMPONENT_TYPE(MQTTButtonComponent, "button")
const EntityBase *MQTTButtonComponent::get_entity() const { return this->button_; }

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
