#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "switch/custom_switch.h"
#include "number/custom_number.h"
#include "enums.h"
#include <queue>
#include <Arduino.h>

namespace esphome {
namespace opentherm {

void IRAM_ATTR forward_interrupt();

class OpenThermComponent : public PollingComponent {
 public:
  sensor::Sensor *ch_min_temperature_sensor_{nullptr};
  sensor::Sensor *ch_max_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_min_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_max_temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *modulation_sensor_{nullptr};
  sensor::Sensor *boiler_temperature_sensor_{nullptr};
  sensor::Sensor *return_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *ch_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *dhw_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *cooling_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *flame_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *fault_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *diagnostic_binary_sensor_{nullptr};
  opentherm::CustomSwitch *ch_enabled_switch_{nullptr};
  opentherm::CustomSwitch *dhw_enabled_switch_{nullptr};
  opentherm::CustomSwitch *cooling_enabled_switch_{nullptr};
  opentherm::CustomNumber *ch_setpoint_temperature_number_{nullptr};
  opentherm::CustomNumber *dhw_setpoint_temperature_number_{nullptr};

  OpenThermComponent() = default;

  void setup() override;
  void update() override;
  void loop() override;
  void set_pins(char pin_in, char pin_out);
  void IRAM_ATTR handle_interrupt();

  void set_ch_min_temperature_sensor(sensor::Sensor *sensor) { ch_min_temperature_sensor_ = sensor; }
  void set_ch_max_temperature_sensor(sensor::Sensor *sensor) { ch_max_temperature_sensor_ = sensor; }
  void set_dhw_min_temperature_sensor(sensor::Sensor *sensor) { dhw_min_temperature_sensor_ = sensor; }
  void set_dhw_max_temperature_sensor(sensor::Sensor *sensor) { dhw_max_temperature_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { pressure_sensor_ = sensor; }
  void set_modulation_sensor(sensor::Sensor *sensor) { modulation_sensor_ = sensor; }
  void set_boiler_temperature_sensor(sensor::Sensor *sensor) { boiler_temperature_sensor_ = sensor; }
  void set_return_temperature_sensor(sensor::Sensor *sensor) { return_temperature_sensor_ = sensor; }
  void set_ch_active_binary_sensor(binary_sensor::BinarySensor *sensor) { ch_active_binary_sensor_ = sensor; }
  void set_dhw_active_binary_sensor(binary_sensor::BinarySensor *sensor) { dhw_active_binary_sensor_ = sensor; }
  void set_cooling_active_binary_sensor(binary_sensor::BinarySensor *sensor) { cooling_active_binary_sensor_ = sensor; }
  void set_flame_active_binary_sensor(binary_sensor::BinarySensor *sensor) { flame_active_binary_sensor_ = sensor; }
  void set_fault_binary_sensor(binary_sensor::BinarySensor *sensor) { fault_binary_sensor_ = sensor; }
  void set_diagnostic_binary_sensor(binary_sensor::BinarySensor *sensor) { diagnostic_binary_sensor_ = sensor; }
  void set_ch_enabled_switch(opentherm::CustomSwitch *switch_) { ch_enabled_switch_ = switch_; }
  void set_dhw_enabled_switch(opentherm::CustomSwitch *switch_) { dhw_enabled_switch_ = switch_; }
  void set_cooling_enabled_switch(opentherm::CustomSwitch *switch_) { cooling_enabled_switch_ = switch_; }
  void set_ch_setpoint_temperature_number(opentherm::CustomNumber *number) {
    ch_setpoint_temperature_number_ = number;
  }
  void set_dhw_setpoint_temperature_number(opentherm::CustomNumber *number) {
    dhw_setpoint_temperature_number_ = number;
  }

 private:
  std::queue<unsigned long> buffer_;
  bool ch_min_max_read = false;
  bool dhw_min_max_read = false;
  float confirmed_dhw_setpoint = 0;
  unsigned long last_millis_ = 0;
  bool wanted_ch_enabled_ = false;
  bool wanted_dhw_enabled_ = false;
  bool wanted_cooling_enabled_ = false;
  int in_pin_ = 0;
  int out_pin_ = 0;
  bool is_slave_ = false;
  volatile unsigned long response_ = 0;
  volatile OpenThermResponseStatus response_status_ = OpenThermResponseStatus::NONE;
  volatile OpenThermStatus status = OpenThermStatus::NOT_INITIALIZED;
  volatile unsigned long response_timestamp_ = 0;
  volatile uint8_t response_bit_index_ = 0;

  void process_status();
  void enqueue_request(unsigned long request);
  const char *format_message_type(unsigned long message);
  void log_message(esp_log_level_t level, const char *pre_message, unsigned long message);
  void publish_sensor_state(sensor::Sensor *sensor, float state);
  void publish_binary_sensor_state(binary_sensor::BinarySensor *sensor, bool state);
  void response_callback(unsigned long response, OpenThermResponseStatus response_status);
  bool IRAM_ATTR is_ready();
  bool send_response(unsigned long request);
  bool send_request_async(unsigned long request);
  unsigned long build_request(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  unsigned long build_response(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  void process();
  void end();
  bool parity(unsigned long frame);
  OpenThermMessageType get_message_type(unsigned long message);
  OpenThermMessageID get_data_id(unsigned long frame);
  const char *message_type_to_string(OpenThermMessageType message_type);
  bool is_valid_request(unsigned long request);
  bool is_valid_response(unsigned long response);
  int IRAM_ATTR read_state();
  void set_active_state();
  void set_idle_state();
  void active_boiler();
  void send_bit(bool high);

  bool is_fault(unsigned long response) { return response & 0x1; }
  bool is_central_heating_active(unsigned long response) { return response & 0x2; }
  bool is_hot_water_active(unsigned long response) { return response & 0x4; }
  bool is_flame_on(unsigned long response) { return response & 0x8; }
  bool is_cooling_active(unsigned long response) { return response & 0x10; }
  bool is_diagnostic(unsigned long response) { return response & 0x40; }
  uint16_t get_u_int(const unsigned long response) const {
    const uint16_t u88 = response & 0xffff;
    return u88;
  }
  float get_float(const unsigned long response) const {
    const uint16_t u88 = get_u_int(response);
    const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
    return f;
  }
  unsigned int temperature_to_data(float temperature) {
    if (temperature < 0)
      temperature = 0;
    if (temperature > 100)
      temperature = 100;
    unsigned int data = (unsigned int) (temperature * 256);
    return data;
  }
};

}  // namespace opentherm
}  // namespace esphome
