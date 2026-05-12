#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::internal_temperature {

class InternalTemperatureSensor : public sensor::Sensor, public PollingComponent {
 public:
#if defined(USE_ESP32) || (defined(USE_ZEPHYR) && defined(USE_NRF52))
  void setup() override;
#endif  // USE_ESP32 || (USE_ZEPHYR && USE_NRF52)
  void dump_config() override;

  void update() override;
};

}  // namespace esphome::internal_temperature
