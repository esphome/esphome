#pragma once

#include "esphome/components/number/number.h"

#include "../duco.h"

namespace esphome::duco {

class DucoNightboostTemperature : public DucoDevice, public number::Number {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(float value) override;
};

class DucoNightboostMax : public DucoDevice, public number::Number {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(float value) override;
};

}  // namespace esphome::duco
