#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "switch/custom_switch.h"
#include "number/custom_number.h"
#include "consts.h"
#include "enums.h"
#include <queue>

namespace esphome {
namespace opentherm {

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
  void dump_config() override;
  static void handle_interrupt(OpenThermComponent *component);
  void set_pins(InternalGPIOPin *read_pin, InternalGPIOPin *write_pin);

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
  void set_ch_enabled_switch(opentherm::CustomSwitch *custom_switch) { ch_enabled_switch_ = custom_switch; }
  void set_dhw_enabled_switch(opentherm::CustomSwitch *custom_switch) { dhw_enabled_switch_ = custom_switch; }
  void set_cooling_enabled_switch(opentherm::CustomSwitch *custom_switch) { cooling_enabled_switch_ = custom_switch; }
  void set_ch_setpoint_temperature_number(opentherm::CustomNumber *number) { ch_setpoint_temperature_number_ = number; }
  void set_dhw_setpoint_temperature_number(opentherm::CustomNumber *number) {
    dhw_setpoint_temperature_number_ = number;
  }

 private:
  InternalGPIOPin *read_pin_;
  InternalGPIOPin *write_pin_;
  ISRInternalGPIOPin isr_read_pin_;

  std::queue<uint32_t> buffer_;
  bool ch_min_max_read_ = false;
  bool dhw_min_max_read_ = false;
  float confirmed_dhw_setpoint_ = 0;
  uint32_t last_millis_ = 0;
  bool wanted_ch_enabled_ = false;
  bool wanted_dhw_enabled_ = false;
  bool wanted_cooling_enabled_ = false;
  volatile uint32_t response_ = 0;
  volatile OpenThermResponseStatus response_status_ = OpenThermResponseStatus::NONE;
  volatile OpenThermStatus status_ = OpenThermStatus::NOT_INITIALIZED;
  volatile uint32_t response_timestamp_ = 0;
  volatile uint8_t response_bit_index_ = 0;

  void set_boiler_status_();

  void enqueue_request_(uint32_t request);
  bool send_request_async_(uint32_t request);
  void process_response_(uint32_t response, OpenThermResponseStatus response_status);

  OpenThermMessageType get_message_type_(uint32_t message);
  OpenThermMessageID get_data_id_(uint32_t frame);

  void log_message_(uint8_t level, const char *pre_message, uint32_t message);
  const char *format_message_type_(uint32_t message);
  const char *message_type_to_string_(OpenThermMessageType message_type);

  void publish_sensor_state_(sensor::Sensor *sensor, float state);
  void publish_binary_sensor_state_(binary_sensor::BinarySensor *sensor, bool state);

  uint32_t build_request_(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  uint32_t build_response_(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  void process_();
  bool parity_(uint32_t frame);
  void set_active_state_();
  void set_idle_state_();
  void send_bit_(bool high);

  bool is_valid_response_(uint32_t response);
  bool is_fault_(uint32_t response) { return response & 0x1; }
  bool is_central_heating_active_(uint32_t response) { return response & 0x2; }
  bool is_hot_water_active_(uint32_t response) { return response & 0x4; }
  bool is_flame_on_(uint32_t response) { return response & 0x8; }
  bool is_cooling_active_(uint32_t response) { return response & 0x10; }
  bool is_diagnostic_(uint32_t response) { return response & 0x40; }
  uint16_t get_uint16_(const uint32_t response) const {
    const uint16_t u88 = response & 0xffff;
    return u88;
  }
  float get_float_(const uint32_t response) const {
    const uint16_t u88 = get_uint16_(response);
    const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
    return f;
  }
  unsigned int temperature_to_data_(float temperature) {
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
