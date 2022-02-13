#include "status_binary_sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/defines.h"

#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace status {

static const char *const TAG = "status";

void StatusBinarySensor::loop() {
  bool status = network::is_connected();
#ifdef USE_MQTT
  if (mqtt::global_mqtt_client != nullptr) {
    status = status && mqtt::global_mqtt_client->is_connected();
  }
#endif
#ifdef USE_API
  if (api::global_api_server != nullptr) {
    status = status && api::global_api_server->is_connected();
  }
#endif

  this->publish_state(status);
}
void StatusBinarySensor::setup() { this->publish_initial_state(false); }
void StatusBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Status Binary Sensor", this); }

}  // namespace status
}  // namespace esphome
