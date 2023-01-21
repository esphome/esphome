#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace cpu_temperature {

class CPUTemperatureSensor : public sensor::Sensor, public PollingComponent {
 public:
  void dump_config() override;

  void update() override;

  std::string unique_id() override;
};

}  // namespace cpu_temperature
}  // namespace esphome
