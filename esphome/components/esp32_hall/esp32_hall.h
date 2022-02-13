#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_hall {

class ESP32HallSensor : public sensor::Sensor, public PollingComponent {
 public:
  void dump_config() override;

  void update() override;

  std::string unique_id() override;
};

}  // namespace esp32_hall
}  // namespace esphome

#endif
