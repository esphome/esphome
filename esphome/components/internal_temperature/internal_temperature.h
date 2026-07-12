#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

// Every ESP32 variant except the original one exposes the on-chip sensor through
// the IDF temperature_sensor driver (the original uses the legacy temprature_sens_read).
#if defined(USE_ESP32) && !defined(USE_ESP32_VARIANT_ESP32)
#include "driver/temperature_sensor.h"
#endif

namespace esphome::internal_temperature {

class InternalTemperatureSensor final : public sensor::Sensor, public PollingComponent {
 public:
#if defined(USE_ESP32) || (defined(USE_ZEPHYR) && defined(USE_NRF52))
  void setup() override;
#endif  // USE_ESP32 || (USE_ZEPHYR && USE_NRF52)
  void dump_config() override;

  void update() override;

#if defined(USE_ESP32) && !defined(USE_ESP32_VARIANT_ESP32)
 protected:
  temperature_sensor_handle_t tsens_{nullptr};
#endif
};

}  // namespace esphome::internal_temperature
