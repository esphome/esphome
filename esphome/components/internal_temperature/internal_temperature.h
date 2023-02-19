#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace internal_temperature {

class InternalTemperatureSensor : public sensor::Sensor, public PollingComponent {
 public:
  void dump_config() override;

  void update();

  std::string unique_id();
};

}  // namespace internal_temperature
}  // namespace esphome
