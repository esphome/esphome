#pragma once
#include "../bthome_handler.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bthome {

class BTHomeSensor : public BTHomeObjectHandler, public esphome::sensor::Sensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

}  // namespace bthome
}  // namespace esphome
