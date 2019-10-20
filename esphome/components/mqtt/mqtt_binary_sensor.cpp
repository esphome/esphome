#include "mqtt_binary_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_BINARY_SENSOR

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.binary_sensor";

std::string MQTTBinarySensorComponent::component_type() const { return "binary_sensor"; }

void MQTTBinarySensorComponent::setup() {
  this->binary_sensor_->add_on_state_callback([this](bool state) { this->publish_state(state); });
}

void MQTTBinarySensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Binary Sensor '%s':", this->binary_sensor_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false)
}
MQTTBinarySensorComponent::MQTTBinarySensorComponent(binary_sensor::BinarySensor *binary_sensor)
    : MQTTComponent(), binary_sensor_(binary_sensor) {
  if (this->binary_sensor_->is_status_binary_sensor()) {
    this->set_custom_state_topic(mqtt::global_mqtt_client->get_availability().topic);
  }
}
std::string MQTTBinarySensorComponent::friendly_name() const { return this->binary_sensor_->get_name(); }

void MQTTBinarySensorComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  if (!this->binary_sensor_->get_device_class().empty())
    root["device_class"] = this->binary_sensor_->get_device_class();
  if (this->binary_sensor_->is_status_binary_sensor())
    root["payload_on"] = mqtt::global_mqtt_client->get_availability().payload_available;
  if (this->binary_sensor_->is_status_binary_sensor())
    root["payload_off"] = mqtt::global_mqtt_client->get_availability().payload_not_available;
  config.command_topic = false;
}
bool MQTTBinarySensorComponent::send_initial_state() {
  if (this->binary_sensor_->has_state()) {
    return this->publish_state(this->binary_sensor_->state);
  } else {
    return true;
  }
}
bool MQTTBinarySensorComponent::is_internal() { return this->binary_sensor_->is_internal(); }
bool MQTTBinarySensorComponent::publish_state(bool state) {
  if (this->binary_sensor_->is_status_binary_sensor())
    return true;

  const char *state_s = state ? "ON" : "OFF";
  return this->publish(this->get_state_topic_(), state_s);
}

}  // namespace mqtt
}  // namespace esphome

#endif
