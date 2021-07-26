#include "mqtt_select.h"
#include "esphome/core/log.h"

#ifdef USE_SELECT

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.select";

using namespace esphome::select;

MQTTSelectComponent::MQTTSelectComponent(Select *select) : MQTTComponent(), select_(select) {}

void MQTTSelectComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &state) {
    auto call = this->select_->make_call();
    call.set_value(state);
    call.perform();
  });
  this->select_->add_on_state_callback([this](const std::string &state) { this->publish_state(state); });
}

void MQTTSelectComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Select '%s':", this->select_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTSelectComponent::component_type() const { return "select"; }

std::string MQTTSelectComponent::friendly_name() const { return this->select_->get_name(); }
void MQTTSelectComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  const auto &traits = select_->traits;
  // https://www.home-assistant.io/integrations/select.mqtt/
  if (!traits.get_icon().empty())
    root["icon"] = traits.get_icon();
  root["options"] = traits.get_options();

  config.command_topic = true;
}
bool MQTTSelectComponent::send_initial_state() {
  if (this->select_->has_state()) {
    return this->publish_state(this->select_->state);
  } else {
    return true;
  }
}
bool MQTTSelectComponent::is_internal() { return this->select_->is_internal(); }
bool MQTTSelectComponent::publish_state(const std::string &value) {
  return this->publish(this->get_state_topic_(), value);
}

}  // namespace mqtt
}  // namespace esphome

#endif
