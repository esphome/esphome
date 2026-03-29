#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace internal_temperature {

class InternalTemperatureSensor : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;

  void update() override;

 protected:
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
  void poll_nrf52_temperature_(uint8_t attempts_left);
#endif  // USE_ZEPHYR && USE_NRF52
};

}  // namespace internal_temperature
}  // namespace esphome
