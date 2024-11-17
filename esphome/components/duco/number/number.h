#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../duco.h"

namespace esphome {
namespace duco {

class DucoComfortTemperature : public DucoDevice, public PollingComponent, public number::Number {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(std::vector<uint8_t> message) override;

  void control(float number) override;
};

}  // namespace duco
}  // namespace esphome
