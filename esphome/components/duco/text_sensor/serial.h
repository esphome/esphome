#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#include "../duco.h"

namespace esphome::duco {

class DucoSerial : public DucoDevice, public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override;
  bool receive_response(const DucoMessage &message) override;
};

}  // namespace esphome::duco
