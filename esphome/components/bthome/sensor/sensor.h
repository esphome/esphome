#pragma once
#include "../remote_object.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::bthome::client {

class BTHomeSensor : public BTHomeRemoteObject, public esphome::sensor::Sensor {
 public:
  bool process_object(const BTHomeObject &object) override;
  void log_config() override;
};

}  // namespace esphome::bthome::client
