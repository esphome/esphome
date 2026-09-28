#pragma once
#include "protocol.h"
#include <cmath>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/gpio.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_LD6004_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_LD6004_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_LD6004_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::ld6004 {
class LD6004Component final : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_stale_timeout(uint32_t timeout) { this->stale_timeout_ = timeout; }
  void set_out_pin(GPIOPin *pin) { this->out_pin_ = pin; }
#ifdef USE_LD6004_SENSOR
  void set_sensor(size_t index, sensor::Sensor *value) { this->sensors_[index] = value; }
#endif
#ifdef USE_LD6004_BINARY_SENSOR
  void set_binary_sensor(size_t index, binary_sensor::BinarySensor *value) { this->binary_[index] = value; }
#endif
#ifdef USE_LD6004_TEXT_SENSOR
  void set_text_sensor(size_t index, text_sensor::TextSensor *value) { this->texts_[index] = value; }
#endif
 protected:
  bool handle_frame_(const Frame &frame, uint32_t now);
  void publish_targets_();
  void publish_sensor_(size_t index, float value);
  void publish_binary_(size_t index, bool value);
  Parser parser_{};
  Tracker tracker_{};
  Startup startup_{};
  GPIOPin *out_pin_{nullptr};
  uint32_t stale_timeout_{5000};
  uint32_t area_time_{0};
  bool area_valid_{false};
  DiagnosticCounter error_counter_{};
#ifdef USE_LD6004_SENSOR
  std::array<sensor::Sensor *, 52> sensors_{};
#endif
#ifdef USE_LD6004_BINARY_SENSOR
  std::array<binary_sensor::BinarySensor *, 16> binary_{};
#endif
#ifdef USE_LD6004_TEXT_SENSOR
  std::array<text_sensor::TextSensor *, 3> texts_{};
#endif
};
}  // namespace esphome::ld6004
