#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace debug {

class DebugComponent : public PollingComponent {
 public:
  void loop() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_device_info_sensor(text_sensor::TextSensor *device_info) { device_info_ = device_info; }
  void set_free_sensor(sensor::Sensor *free_sensor) { free_sensor_ = free_sensor; }
#ifdef ARDUINO_ARCH_ESP8266
  void set_fragmentation_sensor(sensor::Sensor *fragmentation_sensor) { fragmentation_sensor_ = fragmentation_sensor; }
  void set_block_sensor(sensor::Sensor *block_sensor) { block_sensor_ = block_sensor; }
#endif

 protected:
  uint32_t free_heap_{};

  uint32_t min_free_heap_{UINT32_MAX};
  uint32_t min_heap_fragmentation_{0};
  uint32_t min_heap_block_{UINT32_MAX};

	text_sensor::TextSensor *device_info_{nullptr};
  sensor::Sensor *free_sensor_{nullptr};
#ifdef ARDUINO_ARCH_ESP8266
  sensor::Sensor *fragmentation_sensor_{nullptr};
  sensor::Sensor *block_sensor_{nullptr};
#endif
};

}  // namespace debug
}  // namespace esphome
