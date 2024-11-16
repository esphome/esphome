#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../duco.h"

namespace esphome {
namespace duco {

class DucoSerial : public DucoDevice, public PollingComponent, public text_sensor::TextSensor {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(std::vector<uint8_t> message) override;
};

}  // namespace duco
}  // namespace esphome
