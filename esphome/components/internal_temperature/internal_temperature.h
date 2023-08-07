#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace internal_temperature {

class InternalTemperatureSensor : public sensor::Sensor, public PollingComponent {
 public:
  void dump_config() override;

  void update() override;
};

}  // namespace internal_temperature
}  // namespace esphome
