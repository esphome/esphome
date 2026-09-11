#pragma once
#include "../remote_object.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::bthome::client {

class BTHomeSensor : public BTHomeRemoteObject, public esphome::sensor::Sensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

}  // namespace esphome::bthome::client
