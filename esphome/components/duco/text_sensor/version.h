#pragma once

#include "esphome/components/text_sensor/text_sensor.h"

#include "../duco.h"

namespace esphome::duco {

class DucoVersion : public DucoDevice, public text_sensor::TextSensor {
 public:
  void set_address(uint8_t address) { this->address_ = address; }
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  uint8_t address_;
};

}  // namespace esphome::duco
