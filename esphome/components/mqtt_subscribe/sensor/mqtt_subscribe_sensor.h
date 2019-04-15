#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome {
namespace mqtt_subscribe {

class MQTTSubscribeSensor : public sensor::Sensor, public Component {
 public:
  MQTTSubscribeSensor(const std::string &name, mqtt::MQTTClientComponent *parent, const std::string &topic)
    : Sensor(name), parent_(parent), topic_(topic) {}

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
