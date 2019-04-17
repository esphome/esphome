#include "mqtt_text_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_TEXT_SENSOR

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.text_sensor";

using namespace esphome::text_sensor;

MQTTTextSensor::MQTTTextSensor(TextSensor *sensor) : MQTTComponent(), sensor_(sensor) {}
void MQTTTextSensor::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  if (!this->sensor_->get_icon().empty())
    root["icon"] = this->sensor_->get_icon();

  if (!this->sensor_->unique_id().empty())
    root["unique_id"] = this->sensor_->unique_id();

  config.command_topic = false;
}
void MQTTTextSensor::setup() {
  this->sensor_->add_on_state_callback([this](const std::string &state) { this->publish_state(state); });
}

void MQTTTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Text Sensor '%s':", this->sensor_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false);
}

bool MQTTTextSensor::publish_state(const std::string &value) { return this->publish(this->get_state_topic_(), value); }
bool MQTTTextSensor::send_initial_state() {
  if (this->sensor_->has_state()) {
    return this->publish_state(this->sensor_->state);
  } else {
    return true;
  }
}
bool MQTTTextSensor::is_internal() { return this->sensor_->is_internal(); }
std::string MQTTTextSensor::component_type() const { return "sensor"; }
std::string MQTTTextSensor::friendly_name() const { return this->sensor_->get_name(); }

}  // namespace mqtt
}  // namespace esphome

#endif
