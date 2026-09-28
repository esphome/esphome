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
#ifdef USE_LD6004_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_LD6004_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_LD6004_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::ld6004 {
class LD6004Component final : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_stale_timeout(uint32_t timeout) { this->stale_timeout_ = timeout; }
  void set_out_pin(GPIOPin *pin) { this->out_pin_ = pin; }
  bool control(uint8_t value, uint16_t report = 0, bool retry = true);
  bool set_number(uint8_t index, float value);
  bool set_select(uint8_t index, size_t value);
  bool set_output(bool value);
  bool set_zone(uint32_t id, float x_min, float x_max, float y_min, float y_max, float z_min, float z_max);
  bool set_z_range(float minimum, float maximum);
  void refresh();
  void recover_commands();
#ifdef USE_LD6004_SENSOR
  void set_sensor(size_t index, sensor::Sensor *value) { this->sensors_[index] = value; }
#endif
#ifdef USE_LD6004_BINARY_SENSOR
  void set_binary_sensor(size_t index, binary_sensor::BinarySensor *value) { this->binary_[index] = value; }
#endif
#ifdef USE_LD6004_TEXT_SENSOR
  void set_text_sensor(size_t index, text_sensor::TextSensor *value) { this->texts_[index] = value; }
#endif
#ifdef USE_LD6004_NUMBER
  void set_number_entity(size_t index, number::Number *value) { this->numbers_[index] = value; }
#endif
#ifdef USE_LD6004_SELECT
  void set_select_entity(size_t index, select::Select *value) { this->selects_[index] = value; }
#endif
#ifdef USE_LD6004_SWITCH
  void set_target_output_switch(switch_::Switch *value) { this->target_output_switch_ = value; }
#endif
 protected:
  bool enqueue_(const Command &command);
  bool handle_frame_(const Frame &frame, uint32_t now);
  void publish_targets_();
  void publish_sensor_(size_t index, float value);
  void publish_binary_(size_t index, bool value);
  void publish_number_(size_t index, float value);
  void publish_select_(size_t index, uint8_t value);
  Parser parser_{};
  Tracker tracker_{};
  CommandQueue queue_{};
  GPIOPin *out_pin_{nullptr};
  uint32_t stale_timeout_{5000};
  uint32_t area_time_{0};
  bool area_valid_{false};
  DiagnosticCounter error_counter_{};
  DiagnosticCounter command_status_counter_{};
  uint8_t refresh_index_{255};
  uint32_t last_failures_{0};
#ifdef USE_LD6004_SENSOR
  std::array<sensor::Sensor *, 124> sensors_{};
#endif
#ifdef USE_LD6004_BINARY_SENSOR
  std::array<binary_sensor::BinarySensor *, 16> binary_{};
#endif
#ifdef USE_LD6004_TEXT_SENSOR
  std::array<text_sensor::TextSensor *, 4> texts_{};
#endif
#ifdef USE_LD6004_NUMBER
  std::array<number::Number *, 6> numbers_{};
#endif
#ifdef USE_LD6004_SELECT
  std::array<select::Select *, 5> selects_{};
#endif
#ifdef USE_LD6004_SWITCH
  switch_::Switch *target_output_switch_{nullptr};
#endif
};
}  // namespace esphome::ld6004
