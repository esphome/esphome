#pragma once

#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../duco.h"

namespace esphome {
namespace duco {

class DucoSensor : public DucoDevice, public PollingComponent, public sensor::Sensor {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(std::vector<uint8_t> message) override;

 protected:
  uint8_t attempts = 0;
};

}  // namespace duco
}  // namespace esphome
