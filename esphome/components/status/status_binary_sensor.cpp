#include "status_binary_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace status {

static const char *TAG = "status";

StatusBinarySensor::StatusBinarySensor(const std::string &name) : BinarySensor(name) {}
void StatusBinarySensor::loop() {
  bool status = network_is_connected();
#ifdef USE_MQTT
  if (mqtt::global_mqtt_client != nullptr) {
    status = mqtt::global_mqtt_client->is_connected();
  }
#endif
#ifdef USE_API
  if (api::global_api_server != nullptr) {
    status = api::global_api_server->is_connected();
  }
#endif

  if (this->last_status_ != status) {
    this->publish_state(status);
    this->last_status_ = status;
  }
}
void StatusBinarySensor::setup() { this->publish_state(false); }
void StatusBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Status Binary Sensor", this); }


}  // namespace status
}  // namespace esphome
