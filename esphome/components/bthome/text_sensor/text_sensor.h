#pragma once
#include "../remote_object.h"
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace bthome {

class BTHomeTextSensor : public BTHomeRemoteObject, public esphome::text_sensor::TextSensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

}  // namespace bthome
}  // namespace esphome
