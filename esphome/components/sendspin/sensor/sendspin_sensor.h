#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_SENSOR)

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/core/component.h"

namespace esphome {
namespace sendspin {

class SendspinSensor : public Component, public sensor::Sensor, public Parented<SendspinHub> {
 public:
  void dump_config() override;
  void setup() override;

  void set_sensor_type(SendspinSensorTypes sensor_type) { this->sensor_type_ = sensor_type; }

 protected:
#ifdef USE_SENDSPIN_METADATA
  void publish_if_changed_(float value);
  void schedule_publish_(const ServerMetadataStateObject &metadata, float value);
#endif

  SendspinSensorTypes sensor_type_;
};

}  // namespace sendspin
}  // namespace esphome
#endif
