#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../hoermann_hcp.h"

namespace esphome::hoermann_hcp {

class HoermannHcpDoorStateTextSensor : public text_sensor::TextSensor, public Component {
 public:
  explicit HoermannHcpDoorStateTextSensor(HoermannHcp *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

 protected:
  void update_from_state_();

  HoermannHcp *const parent_;
  DoorState published_state_{DoorState::CLOSED};
  bool published_{false};
};

}  // namespace esphome::hoermann_hcp
