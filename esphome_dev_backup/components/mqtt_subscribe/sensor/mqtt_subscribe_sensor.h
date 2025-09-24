#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome {
namespace mqtt_subscribe {

class MQTTSubscribeSensor : public sensor::Sensor, public Component {
 public:
  void set_parent(mqtt::MQTTClientComponent *parent) { parent_ = parent; }
  void set_topic(const std::string &topic) { topic_ = topic; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_qos(uint8_t qos);

 protected:
  mqtt::MQTTClientComponent *parent_;
  std::string topic_;
  uint8_t qos_{0};
};

}  // namespace mqtt_subscribe
}  // namespace esphome

#endif  // USE_MQTT
