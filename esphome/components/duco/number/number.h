#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

#include "../duco.h"

namespace esphome::duco {

class DucoComfortTemperature : public DucoDevice, public PollingComponent, public number::Number {
 public:
  void update() override;
  bool receive_response(const DucoMessage &message) override;

 protected:
  void control(float value) override;
};

}  // namespace esphome::duco
