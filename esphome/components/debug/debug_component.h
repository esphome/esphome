#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/macros.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace debug {

class DebugComponent : public PollingComponent {
 public:
  void loop() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;

#ifdef USE_TEXT_SENSOR
  void set_device_info_sensor(text_sensor::TextSensor *device_info) { device_info_ = device_info; }
  void set_reset_reason_sensor(text_sensor::TextSensor *reset_reason) { reset_reason_ = reset_reason; }
#endif  // USE_TEXT_SENSOR
#ifdef USE_SENSOR
  void set_free_sensor(sensor::Sensor *free_sensor) { free_sensor_ = free_sensor; }
  void set_block_sensor(sensor::Sensor *block_sensor) { block_sensor_ = block_sensor; }
#if defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
  void set_fragmentation_sensor(sensor::Sensor *fragmentation_sensor) { fragmentation_sensor_ = fragmentation_sensor; }
#endif
  void set_loop_time_sensor(sensor::Sensor *loop_time_sensor) { loop_time_sensor_ = loop_time_sensor; }
#ifdef USE_ESP32
  void set_psram_sensor(sensor::Sensor *psram_sensor) { this->psram_sensor_ = psram_sensor; }
#endif  // USE_ESP32
#endif  // USE_SENSOR
 protected:
  uint32_t free_heap_{};

#ifdef USE_SENSOR
  uint32_t last_loop_timetag_{0};
  uint32_t max_loop_time_{0};

  sensor::Sensor *free_sensor_{nullptr};
  sensor::Sensor *block_sensor_{nullptr};
#if defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
  sensor::Sensor *fragmentation_sensor_{nullptr};
#endif
  sensor::Sensor *loop_time_sensor_{nullptr};
#ifdef USE_ESP32
  sensor::Sensor *psram_sensor_{nullptr};
#endif  // USE_ESP32
#endif  // USE_SENSOR

#ifdef USE_ESP32
  /**
   * @brief Logs information about the device's partition table.
   *
   * This function iterates through the ESP32's partition table and logs details
   * about each partition, including its name, type, subtype, starting address,
   * and size. The information is useful for diagnosing issues related to flash
   * memory or verifying the partition configuration dynamically at runtime.
   *
   * Only available when compiled for ESP32 platforms.
   */
  void log_partition_info_();
#endif  // USE_ESP32

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *device_info_{nullptr};
  text_sensor::TextSensor *reset_reason_{nullptr};
#endif  // USE_TEXT_SENSOR

  std::string get_reset_reason_();
  uint32_t get_free_heap_();
  void get_device_info_(std::string &device_info);
  void update_platform_();
};

}  // namespace debug
}  // namespace esphome
