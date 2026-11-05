#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/macros.h"
#include <span>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::debug {

static constexpr size_t DEVICE_INFO_BUFFER_SIZE = 256;
static constexpr size_t RESET_REASON_BUFFER_SIZE = 128;
static constexpr size_t WAKEUP_CAUSE_BUFFER_SIZE = 128;

// buf_append_printf is now provided by esphome/core/helpers.h

class DebugComponent final : public PollingComponent {
 public:
  void loop() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(device_info)
  SUB_TEXT_SENSOR(reset_reason)
#endif  // USE_TEXT_SENSOR
#ifdef USE_SENSOR
  SUB_SENSOR(free)
  SUB_SENSOR(block)
#if defined(USE_ESP8266) || defined(USE_ESP32)
  SUB_SENSOR(fragmentation)
#endif
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  SUB_SENSOR(min_free)
#endif
  SUB_SENSOR(loop_time)
#ifdef USE_ESP32
  SUB_SENSOR(psram)
#endif  // USE_ESP32
  SUB_SENSOR(cpu_frequency)
#endif  // USE_SENSOR
#ifdef USE_ESP32
  void on_shutdown() override;
#endif  // USE_ESP32
 protected:
  uint32_t free_heap_{};

#ifdef USE_SENSOR
  uint32_t last_loop_timetag_{0};
  uint32_t max_loop_time_{0};
#endif  // USE_SENSOR

#if defined(USE_ESP32) || defined(USE_ZEPHYR)
  /**
   * @brief Logs information about the device's partition table.
   *
   * This function iterates through the partition table and logs details
   * about each partition, including its name, type, subtype, starting address,
   * and size. The information is useful for diagnosing issues related to flash
   * memory or verifying the partition configuration dynamically at runtime.
   *
   * Only available when compiled for ESP32 and ZEPHYR platforms.
   */
  void log_partition_info_();
#endif

  const char *get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer);
  const char *get_wakeup_cause_(std::span<char, WAKEUP_CAUSE_BUFFER_SIZE> buffer);
  uint32_t get_free_heap_();
  size_t get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos);
  void update_platform_();
};

}  // namespace esphome::debug
