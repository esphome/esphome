#include "debug_component.h"

#include <algorithm>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/version.h"
#include <cinttypes>
#include <climits>

namespace esphome {
namespace debug {

static const char *const TAG = "debug";

void DebugComponent::dump_config() {
#ifndef ESPHOME_LOG_HAS_DEBUG
  return;  // Can't log below if debug logging is disabled
#endif

  ESP_LOGCONFIG(TAG, "Debug component:");
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Device info", this->device_info_);
#endif  // USE_TEXT_SENSOR
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Free space on heap", this->free_sensor_);
  LOG_SENSOR("  ", "Largest free heap block", this->block_sensor_);
#if defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
  LOG_SENSOR("  ", "Heap fragmentation", this->fragmentation_sensor_);
#endif  // defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 2)
#endif  // USE_SENSOR

  std::string device_info;
  device_info.reserve(256);
  ESP_LOGD(TAG, "ESPHome version %s", ESPHOME_VERSION);
  device_info += ESPHOME_VERSION;

  this->free_heap_ = get_free_heap_();
  ESP_LOGD(TAG, "Free Heap Size: %" PRIu32 " bytes", this->free_heap_);

  get_device_info_(device_info);

#ifdef USE_TEXT_SENSOR
  if (this->device_info_ != nullptr) {
    if (device_info.length() > 255)
      device_info.resize(255);
    this->device_info_->publish_state(device_info);
  }
  if (this->reset_reason_ != nullptr) {
    this->reset_reason_->publish_state(get_reset_reason_());
  }
#endif  // USE_TEXT_SENSOR
}

void DebugComponent::loop() {
  // log when free heap space has halved
  uint32_t new_free_heap = get_free_heap_();
  if (new_free_heap < this->free_heap_ / 2) {
    this->free_heap_ = new_free_heap;
    ESP_LOGD(TAG, "Free Heap Size: %" PRIu32 " bytes", this->free_heap_);
    this->status_momentary_warning("heap", 1000);
  }

#ifdef USE_SENSOR
  // calculate loop time - from last call to this one
  if (this->loop_time_sensor_ != nullptr) {
    uint32_t now = millis();
    uint32_t loop_time = now - this->last_loop_timetag_;
    this->max_loop_time_ = std::max(this->max_loop_time_, loop_time);
    this->last_loop_timetag_ = now;
  }
#endif  // USE_SENSOR
}

void DebugComponent::update() {
#ifdef USE_SENSOR
  if (this->free_sensor_ != nullptr) {
    this->free_sensor_->publish_state(get_free_heap_());
  }

  if (this->loop_time_sensor_ != nullptr) {
    this->loop_time_sensor_->publish_state(this->max_loop_time_);
    this->max_loop_time_ = 0;
  }

#endif  // USE_SENSOR
  update_platform_();
}

float DebugComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace debug
}  // namespace esphome
