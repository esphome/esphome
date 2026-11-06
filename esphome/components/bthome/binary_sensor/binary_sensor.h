#pragma once
#include "../remote_object.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::bthome::client {

class BTHomeBinarySensor : public BTHomeRemoteObject, public esphome::binary_sensor::BinarySensor {
 public:
  bool process_object(const BTHomeObject &object) override;
  void log_config() override;
};

}  // namespace esphome::bthome::client
