#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "switch/custom_switch.h"
#endif
#ifdef USE_NUMBER
#include "number/custom_number.h"
#endif
#include "consts.h"
#include "enums.h"
#include <queue>

namespace esphome {
namespace opentherm {

class OpenThermComponent : public PollingComponent {
 public:
#ifdef USE_SENSOR
  sensor::Sensor *ch_min_temperature_sensor_{nullptr};
  sensor::Sensor *ch_max_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_min_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_max_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_flow_rate_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *modulation_sensor_{nullptr};
  sensor::Sensor *dhw_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_2_temperature_sensor_{nullptr};
  sensor::Sensor *boiler_temperature_sensor_{nullptr};
  sensor::Sensor *boiler_2_temperature_sensor_{nullptr};
  sensor::Sensor *return_temperature_sensor_{nullptr};
  sensor::Sensor *outside_temperature_sensor_{nullptr};
  sensor::Sensor *exhaust_temperature_sensor_{nullptr};
  sensor::Sensor *oem_error_code_sensor_{nullptr};
  sensor::Sensor *oem_diagnostic_code_sensor_{nullptr};
  sensor::Sensor *burner_starts_sensor_{nullptr};
  sensor::Sensor *burner_ops_hours_sensor_{nullptr};
  sensor::Sensor *ch_pump_starts_sensor_{nullptr};
  sensor::Sensor *ch_pump_ops_hours_sensor_{nullptr};
  sensor::Sensor *dhw_pump_valve_starts_sensor_{nullptr};
  sensor::Sensor *dhw_pump_valve_ops_hours_sensor_{nullptr};
  sensor::Sensor *dhw_burner_starts_sensor_{nullptr};
  sensor::Sensor *dhw_burner_ops_hours_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *ch_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *ch_2_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *dhw_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *cooling_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *flame_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *fault_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *diagnostic_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *service_request_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *lockout_reset_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *water_pressure_fault_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *gas_flame_fault_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *air_pressure_fault_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *water_over_temperature_fault_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *dhw_present_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *modulating_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *cooling_supported_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *dhw_storage_tank_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *device_lowoff_pump_control_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *ch_2_present_binary_sensor_{nullptr};
#endif
#ifdef USE_SWITCH
  opentherm::CustomSwitch *ch_enabled_switch_{nullptr};
  opentherm::CustomSwitch *ch_2_enabled_switch_{nullptr};
  opentherm::CustomSwitch *dhw_enabled_switch_{nullptr};
  opentherm::CustomSwitch *cooling_enabled_switch_{nullptr};
  opentherm::CustomSwitch *otc_active_switch_{nullptr};
#endif
#ifdef USE_NUMBER
  opentherm::CustomNumber *ch_setpoint_temperature_number_{nullptr};
  opentherm::CustomNumber *ch_2_setpoint_temperature_number_{nullptr};
  opentherm::CustomNumber *dhw_setpoint_temperature_number_{nullptr};
#endif

  OpenThermComponent() = default;

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  static void handle_interrupt(OpenThermComponent *component);
  void set_pins(InternalGPIOPin *read_pin, InternalGPIOPin *write_pin);

#ifdef USE_SENSOR
  void set_ch_min_temperature_sensor(sensor::Sensor *sensor) { ch_min_temperature_sensor_ = sensor; }
  void set_ch_max_temperature_sensor(sensor::Sensor *sensor) { ch_max_temperature_sensor_ = sensor; }
  void set_dhw_min_temperature_sensor(sensor::Sensor *sensor) { dhw_min_temperature_sensor_ = sensor; }
  void set_dhw_max_temperature_sensor(sensor::Sensor *sensor) { dhw_max_temperature_sensor_ = sensor; }
  void set_dhw_flow_rate_sensor(sensor::Sensor *sensor) { dhw_flow_rate_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { pressure_sensor_ = sensor; }
  void set_modulation_sensor(sensor::Sensor *sensor) { modulation_sensor_ = sensor; }
  void set_dhw_temperature_sensor(sensor::Sensor *sensor) { dhw_temperature_sensor_ = sensor; }
  void set_dhw_2_temperature_sensor(sensor::Sensor *sensor) { dhw_2_temperature_sensor_ = sensor; }
  void set_boiler_temperature_sensor(sensor::Sensor *sensor) { boiler_temperature_sensor_ = sensor; }
  void set_boiler_2_temperature_sensor(sensor::Sensor *sensor) { boiler_2_temperature_sensor_ = sensor; }
  void set_return_temperature_sensor(sensor::Sensor *sensor) { return_temperature_sensor_ = sensor; }
  void set_outside_temperature_sensor(sensor::Sensor *sensor) { outside_temperature_sensor_ = sensor; }
  void set_exhaust_temperature_sensor(sensor::Sensor *sensor) { exhaust_temperature_sensor_ = sensor; }
  void set_oem_error_code_sensor(sensor::Sensor *sensor) { oem_error_code_sensor_ = sensor; }
  void set_oem_diagnostic_code_sensor(sensor::Sensor *sensor) { oem_diagnostic_code_sensor_ = sensor; }
  void set_burner_starts_sensor(sensor::Sensor *sensor) { burner_starts_sensor_ = sensor; }
  void set_burner_ops_hours_sensor(sensor::Sensor *sensor) { burner_ops_hours_sensor_ = sensor; }
  void set_ch_pump_starts_sensor(sensor::Sensor *sensor) { ch_pump_starts_sensor_ = sensor; }
  void set_ch_pump_ops_hours_sensor(sensor::Sensor *sensor) { ch_pump_ops_hours_sensor_ = sensor; }
  void set_dhw_pump_valve_starts_sensor(sensor::Sensor *sensor) { dhw_pump_valve_starts_sensor_ = sensor; }
  void set_dhw_pump_valve_ops_hours_sensor(sensor::Sensor *sensor) { dhw_pump_valve_ops_hours_sensor_ = sensor; }
  void set_dhw_burner_starts_sensor(sensor::Sensor *sensor) { dhw_burner_starts_sensor_ = sensor; }
  void set_dhw_burner_ops_hours_sensor(sensor::Sensor *sensor) { dhw_burner_ops_hours_sensor_ = sensor; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_ch_active_binary_sensor(binary_sensor::BinarySensor *sensor) { ch_active_binary_sensor_ = sensor; }
  void set_ch_2_active_binary_sensor(binary_sensor::BinarySensor *sensor) { ch_2_active_binary_sensor_ = sensor; }
  void set_dhw_active_binary_sensor(binary_sensor::BinarySensor *sensor) { dhw_active_binary_sensor_ = sensor; }
  void set_cooling_active_binary_sensor(binary_sensor::BinarySensor *sensor) { cooling_active_binary_sensor_ = sensor; }
  void set_flame_active_binary_sensor(binary_sensor::BinarySensor *sensor) { flame_active_binary_sensor_ = sensor; }
  void set_fault_binary_sensor(binary_sensor::BinarySensor *sensor) { fault_binary_sensor_ = sensor; }
  void set_diagnostic_binary_sensor(binary_sensor::BinarySensor *sensor) { diagnostic_binary_sensor_ = sensor; }
  void set_service_request_binary_sensor(binary_sensor::BinarySensor *sensor) {
    service_request_binary_sensor_ = sensor;
  }
  void set_lockout_reset_binary_sensor(binary_sensor::BinarySensor *sensor) { lockout_reset_binary_sensor_ = sensor; }
  void set_water_pressure_fault_binary_sensor(binary_sensor::BinarySensor *sensor) {
    water_pressure_fault_binary_sensor_ = sensor;
  }
  void set_gas_flame_fault_binary_sensor(binary_sensor::BinarySensor *sensor) {
    gas_flame_fault_binary_sensor_ = sensor;
  }
  void set_air_pressure_fault_binary_sensor(binary_sensor::BinarySensor *sensor) {
    air_pressure_fault_binary_sensor_ = sensor;
  }
  void set_water_over_temperature_fault_binary_sensor(binary_sensor::BinarySensor *sensor) {
    water_over_temperature_fault_binary_sensor_ = sensor;
  }
  void set_dhw_present_binary_sensor(binary_sensor::BinarySensor *sensor) { dhw_present_binary_sensor_ = sensor; }
  void set_modulating_binary_sensor(binary_sensor::BinarySensor *sensor) { modulating_binary_sensor_ = sensor; }
  void set_cooling_supported_binary_sensor(binary_sensor::BinarySensor *sensor) {
    cooling_supported_binary_sensor_ = sensor;
  }
  void set_dhw_storage_tank_binary_sensor(binary_sensor::BinarySensor *sensor) {
    dhw_storage_tank_binary_sensor_ = sensor;
  }
  void set_device_lowoff_pump_control_binary_sensor(binary_sensor::BinarySensor *sensor) {
    device_lowoff_pump_control_binary_sensor_ = sensor;
  }
  void set_ch_2_present_binary_sensor(binary_sensor::BinarySensor *sensor) { ch_2_present_binary_sensor_ = sensor; }
#endif
#ifdef USE_SWITCH
  void set_ch_enabled_switch(opentherm::CustomSwitch *custom_switch) { ch_enabled_switch_ = custom_switch; }
  void set_ch_2_enabled_switch(opentherm::CustomSwitch *custom_switch) { ch_2_enabled_switch_ = custom_switch; }
  void set_dhw_enabled_switch(opentherm::CustomSwitch *custom_switch) { dhw_enabled_switch_ = custom_switch; }
  void set_cooling_enabled_switch(opentherm::CustomSwitch *custom_switch) { cooling_enabled_switch_ = custom_switch; }
  void set_otc_active_switch(opentherm::CustomSwitch *custom_switch) { otc_active_switch_ = custom_switch; }
#endif
#ifdef USE_NUMBER
  void set_ch_setpoint_temperature_number(opentherm::CustomNumber *number) { ch_setpoint_temperature_number_ = number; }
  void set_ch_2_setpoint_temperature_number(opentherm::CustomNumber *number) {
    ch_2_setpoint_temperature_number_ = number;
  }
  void set_dhw_setpoint_temperature_number(opentherm::CustomNumber *number) {
    dhw_setpoint_temperature_number_ = number;
  }
#endif

 private:
  InternalGPIOPin *read_pin_;
  InternalGPIOPin *write_pin_;
  ISRInternalGPIOPin isr_read_pin_;

  std::queue<uint32_t> buffer_;
  float confirmed_dhw_setpoint_ = 0;
  bool wanted_ch_enabled_ = false;
  bool wanted_ch_2_enabled_ = false;
  bool wanted_dhw_enabled_ = false;
  bool wanted_cooling_enabled_ = false;
  bool wanted_otc_active_ = false;
  volatile uint32_t response_ = 0;
  volatile OpenThermResponseStatus response_status_ = OpenThermResponseStatus::NONE;
  volatile OpenThermStatus status_ = OpenThermStatus::NOT_INITIALIZED;
  volatile uint32_t response_timestamp_ = 0;
  volatile uint8_t response_bit_index_ = 0;
  uint32_t last_millis_ = 0;
  uint32_t start_millis_ = 0;
  uint16_t start_interval_ = 0;

  uint32_t last_millis_return_water_temp_ = 0;
  uint32_t last_millis_boiler_water_temp_ = 0;
  uint32_t last_millis_boiler_2_water_temp_ = 0;
  uint32_t last_millis_flow_rate_ = 0;
  uint32_t last_millis_ch_pressure_ = 0;
  uint32_t last_millis_rel_mod_level_ = 0;
  uint32_t last_millis_dhw_temp_ = 0;
  uint32_t last_millis_dhw_2_temp_ = 0;
  uint32_t last_millis_outside_temp_ = 0;
  uint32_t last_millis_exhaust_temp_ = 0;
  uint32_t last_millis_ch_max_min_temp_ = 0;
  uint32_t last_millis_dhw_max_min_temp_ = 0;
  uint32_t last_millis_oem_diagnostic_code_ = 0;
  uint32_t last_millis_fault_flags_ = 0;
  uint32_t last_millis_param_flags_ = 0;
  uint32_t last_millis_burner_starts_ = 0;
  uint32_t last_millis_burner_ops_hours_ = 0;
  uint32_t last_millis_ch_pump_starts_ = 0;
  uint32_t last_millis_ch_pump_ops_hours_ = 0;
  uint32_t last_millis_dhw_pump_valve_starts_ = 0;
  uint32_t last_millis_dhw_pump_valve_ops_hours_ = 0;
  uint32_t last_millis_dhw_burner_starts_ = 0;
  uint32_t last_millis_dhw_burner_ops_hours_ = 0;
  uint32_t last_millis_boiler_configuration_ = 0;

  void update_spread_();

  void request_(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  void set_boiler_status_();

  void enqueue_request_(uint32_t request);
  bool send_request_async_(uint32_t request);
  void process_response_(uint32_t response, OpenThermResponseStatus response_status);

  OpenThermMessageType get_message_type_(uint32_t message);
  OpenThermMessageID get_data_id_(uint32_t frame);

  void log_message_(uint8_t level, const char *pre_message, uint32_t message);
  const char *format_message_type_(uint32_t message);
  const char *message_type_to_string_(OpenThermMessageType message_type);

#ifdef USE_SENSOR
  void publish_sensor_state_(sensor::Sensor *sensor, float state);
#endif
#ifdef USE_BINARY_SENSOR
  void publish_binary_sensor_state_(binary_sensor::BinarySensor *sensor, bool state);
#endif

  uint32_t build_request_(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  uint32_t build_response_(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  void process_();
  bool parity_(uint32_t frame);
  void set_active_state_();
  void set_idle_state_();
  void send_bit_(bool high);
  bool is_elapsed_(uint32_t &last_millis, uint16_t interval) const {
    uint32_t current_millis = millis();
    if (last_millis == 0 || current_millis < last_millis || current_millis - last_millis > interval) {
      last_millis = current_millis;
      return true;
    }
    return false;
  }
  bool can_start_(uint32_t last_millis, uint8_t index);
  bool should_request_(uint32_t &last_millis, uint8_t index);

  bool is_valid_response_(uint32_t response);
  uint16_t get_uint16_(const uint32_t response) const {
    const uint16_t u88 = response & 0xffff;
    return u88;
  }
  int16_t get_int16_(const uint32_t response) const {
    const uint16_t u88 = get_uint16_(response);
    return (int16_t) u88;
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
