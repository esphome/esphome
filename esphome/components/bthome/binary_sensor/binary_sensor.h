#pragma once
#include "../remote_object.h"
#include "../local_object.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bthome {

class BTHomeBinarySensor : public BTHomeRemoteObject, public esphome::binary_sensor::BinarySensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

namespace server {}  // namespace server

}  // namespace bthome
}  // namespace esphome
