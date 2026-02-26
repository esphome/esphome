#pragma once
#include "../bthome_handler.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bthome {

class BTHomeBinarySensor : public BTHomeObjectHandler, public esphome::binary_sensor::BinarySensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

}  // namespace bthome
}  // namespace esphome
