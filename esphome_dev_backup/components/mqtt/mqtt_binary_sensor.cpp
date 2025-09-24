#include "mqtt_binary_sensor.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_BINARY_SENSOR

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.binary_sensor";

std::string MQTTBinarySensorComponent::component_type() const { return "binary_sensor"; }
const EntityBase *MQTTBinarySensorComponent::get_entity() const { return this->binary_sensor_; }

void MQTTBinarySensorComponent::setup() {
  this->binary_sensor_->add_on_state_callback([this](bool state) { this->publish_state(state); });
}

void MQTTBinarySensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Binary Sensor '%s':", this->binary_sensor_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}
MQTTBinarySensorComponent::MQTTBinarySensorComponent(binary_sensor::BinarySensor *binary_sensor)
    : binary_sensor_(binary_sensor) {
  if (this->binary_sensor_->is_status_binary_sensor()) {
    this->set_custom_state_topic(mqtt::global_mqtt_client->get_availability().topic.c_str());
  }
}

void MQTTBinarySensorComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  if (!this->binary_sensor_->get_device_class().empty())
    root[MQTT_DEVICE_CLASS] = this->binary_sensor_->get_device_class();
  if (this->binary_sensor_->is_status_binary_sensor())
    root[MQTT_PAYLOAD_ON] = mqtt::global_mqtt_client->get_availability().payload_available;
  if (this->binary_sensor_->is_status_binary_sensor())
    root[MQTT_PAYLOAD_OFF] = mqtt::global_mqtt_client->get_availability().payload_not_available;
  config.command_topic = false;
}
bool MQTTBinarySensorComponent::send_initial_state() {
  if (this->binary_sensor_->has_state()) {
    return this->publish_state(this->binary_sensor_->state);
  } else {
    return true;
  }
}
bool MQTTBinarySensorComponent::publish_state(bool state) {
  if (this->binary_sensor_->is_status_binary_sensor())
    return true;

  const char *state_s = state ? "ON" : "OFF";
  return this->publish(this->get_state_topic_(), state_s);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
