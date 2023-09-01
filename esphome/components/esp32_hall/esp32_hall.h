#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_hall {

class ESP32HallSensor : public sensor::Sensor, public PollingComponent {
 public:
  void dump_config() override;

  void update() override;
};

}  // namespace esp32_hall
}  // namespace esphome

#endif
