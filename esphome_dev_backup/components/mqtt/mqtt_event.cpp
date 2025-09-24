#include "mqtt_event.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_EVENT

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.event";

using namespace esphome::event;

MQTTEventComponent::MQTTEventComponent(event::Event *event) : event_(event) {}

void MQTTEventComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  JsonArray event_types = root.createNestedArray(MQTT_EVENT_TYPES);
  for (const auto &event_type : this->event_->get_event_types())
    event_types.add(event_type);

  if (!this->event_->get_device_class().empty())
    root[MQTT_DEVICE_CLASS] = this->event_->get_device_class();

  config.command_topic = false;
}

void MQTTEventComponent::setup() {
  this->event_->add_on_event_callback([this](const std::string &event_type) { this->publish_event_(event_type); });
}

void MQTTEventComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Event '%s': ", this->event_->get_name().c_str());
  ESP_LOGCONFIG(TAG, "Event Types: ");
  for (const auto &event_type : this->event_->get_event_types()) {
    ESP_LOGCONFIG(TAG, "- %s", event_type.c_str());
  }
  LOG_MQTT_COMPONENT(true, true);
}

bool MQTTEventComponent::publish_event_(const std::string &event_type) {
  return this->publish_json(this->get_state_topic_(),
                            [event_type](JsonObject root) { root[MQTT_EVENT_TYPE] = event_type; });
}

std::string MQTTEventComponent::component_type() const { return "event"; }
const EntityBase *MQTTEventComponent::get_entity() const { return this->event_; }

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
