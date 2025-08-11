#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_SENSOR)

#include "esphome/components/resonate/resonate_hub.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/core/component.h"

namespace esphome {
namespace resonate {

class ResonateSensor : public Component, public sensor::Sensor, public Parented<ResonateHub> {
 public:
  void dump_config() override;
  void setup() override;

  void set_sensor_type(ResonateSensorTypes sensor_type) { this->sensor_type_ = sensor_type; }

 protected:
  ResonateSensorTypes sensor_type_;
};

}  // namespace resonate
}  // namespace esphome
#endif
