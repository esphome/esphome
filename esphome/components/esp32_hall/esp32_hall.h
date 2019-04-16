#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_hall {

class ESP32HallSensor : public sensor::PollingSensorComponent {
 public:
  explicit ESP32HallSensor(const std::string &name, uint32_t update_interval)
      : sensor::PollingSensorComponent(name, update_interval) {}

  void dump_config() override;

  void update() override;

  std::string unique_id() override;
};

}  // namespace esp32_hall
}  // namespace esphome

#endif
