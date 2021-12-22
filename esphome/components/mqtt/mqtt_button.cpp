#include "mqtt_button.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_BUTTON

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.button";

using namespace esphome::button;

MQTTButtonComponent::MQTTButtonComponent(button::Button *button) : MQTTComponent(), button_(button) {}

void MQTTButtonComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    if (payload == "PRESS") {
      this->button_->press();
    } else {
      ESP_LOGW(TAG, "'%s': Received unknown status payload: %s", this->friendly_name().c_str(), payload.c_str());
      this->status_momentary_warning("state", 5000);
    }
  });
}
void MQTTButtonComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Button '%s': ", this->button_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true);
}

void MQTTButtonComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  config.state_topic = false;
  if (!this->button_->get_device_class().empty())
    root[MQTT_DEVICE_CLASS] = this->button_->get_device_class();
}

std::string MQTTButtonComponent::component_type() const { return "button"; }
const EntityBase *MQTTButtonComponent::get_entity() const { return this->button_; }

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
