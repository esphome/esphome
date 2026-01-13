#include "custom_mqtt_device.h"

#ifdef USE_MQTT

#include "esphome/core/log.h"

namespace esphome::mqtt {

static const char *const TAG = "mqtt.custom";

bool CustomMQTTDevice::publish(const std::string &topic, const std::string &payload, uint8_t qos, bool retain) {
  return global_mqtt_client->publish(topic, payload, qos, retain);
}
bool CustomMQTTDevice::publish(const std::string &topic, float value, int8_t number_decimals) {
  char buf[VALUE_ACCURACY_MAX_LEN];
  value_accuracy_to_buf(buf, value, number_decimals);
  return this->publish(topic, buf);
}
bool CustomMQTTDevice::publish(const std::string &topic, int value) {
  char buffer[24];
  sprintf(buffer, "%d", value);
  return this->publish(topic, buffer);
}
bool CustomMQTTDevice::publish_json(const std::string &topic, const json::json_build_t &f, uint8_t qos, bool retain) {
  return global_mqtt_client->publish_json(topic, f, qos, retain);
}
bool CustomMQTTDevice::publish_json(const std::string &topic, const json::json_build_t &f) {
  return this->publish_json(topic, f, 0, false);
}
bool CustomMQTTDevice::is_connected() { return global_mqtt_client != nullptr && global_mqtt_client->is_connected(); }

}  // namespace esphome::mqtt

#endif  // USE_MQTT
