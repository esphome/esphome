#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace debug {

class DebugComponent : public PollingComponent {
 public:
  void loop() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_free_sensor(sensor::Sensor *free_sensor) { free_sensor_ = free_sensor; }
#ifdef ARDUINO_ARCH_ESP8266
  void set_fragmentation_sensor(sensor::Sensor *fragmentation_sensor) { fragmentation_sensor_ = fragmentation_sensor; }
  void set_block_sensor(sensor::Sensor *block_sensor) { block_sensor_ = block_sensor; }
#endif

 protected:
  uint32_t free_heap_{};
  
  sensor::Sensor *free_sensor_{nullptr};
#ifdef ARDUINO_ARCH_ESP8266
  sensor::Sensor *fragmentation_sensor_{nullptr};
  sensor::Sensor *block_sensor_{nullptr};
#endif
};

}  // namespace debug
}  // namespace esphome
