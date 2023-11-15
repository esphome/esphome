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
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SWITCH
#include "switch/opentherm_switch.h"
#endif
#ifdef USE_NUMBER
#include "number/opentherm_number.h"
#endif
#include "consts.h"
#include "enums.h"
#include <queue>

namespace esphome {
namespace opentherm {

class OpenThermComponent : public PollingComponent {
#ifdef USE_SENSOR
  SUB_SENSOR(ch_min_temperature);
  SUB_SENSOR(ch_max_temperature);
  SUB_SENSOR(dhw_min_temperature);
  SUB_SENSOR(dhw_max_temperature);
  SUB_SENSOR(dhw_flow_rate);
  SUB_SENSOR(pressure);
  SUB_SENSOR(modulation);
  SUB_SENSOR(dhw_temperature);
  SUB_SENSOR(dhw_2_temperature);
  SUB_SENSOR(boiler_temperature);
  SUB_SENSOR(boiler_2_temperature);
  SUB_SENSOR(return_temperature);
  SUB_SENSOR(outside_temperature);
  SUB_SENSOR(exhaust_temperature);
  SUB_SENSOR(oem_error_code);
  SUB_SENSOR(oem_diagnostic_code);
  SUB_SENSOR(burner_starts);
  SUB_SENSOR(burner_ops_hours);
  SUB_SENSOR(ch_pump_starts);
  SUB_SENSOR(ch_pump_ops_hours);
  SUB_SENSOR(dhw_pump_valve_starts);
  SUB_SENSOR(dhw_pump_valve_ops_hours);
  SUB_SENSOR(dhw_burner_starts);
  SUB_SENSOR(dhw_burner_ops_hours);
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(ch_active);
  SUB_BINARY_SENSOR(ch_2_active);
  SUB_BINARY_SENSOR(dhw_active);
  SUB_BINARY_SENSOR(cooling_active);
  SUB_BINARY_SENSOR(flame_active);
  SUB_BINARY_SENSOR(fault);
  SUB_BINARY_SENSOR(diagnostic);
  SUB_BINARY_SENSOR(service_request);
  SUB_BINARY_SENSOR(lockout_reset);
  SUB_BINARY_SENSOR(water_pressure_fault);
  SUB_BINARY_SENSOR(gas_flame_fault);
  SUB_BINARY_SENSOR(air_pressure_fault);
  SUB_BINARY_SENSOR(water_over_temperature_fault);
  SUB_BINARY_SENSOR(dhw_present);
  SUB_BINARY_SENSOR(modulating);
  SUB_BINARY_SENSOR(cooling_supported);
  SUB_BINARY_SENSOR(dhw_storage_tank);
  SUB_BINARY_SENSOR(device_lowoff_pump_control);
  SUB_BINARY_SENSOR(ch_2_present);
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(boiler_lo_reset);
  SUB_BUTTON(ch_water_filling);
#endif
#ifdef USE_SWITCH
  SUB_OPENTHERM_SWITCH(ch_enabled);
  SUB_OPENTHERM_SWITCH(ch_2_enabled);
  SUB_OPENTHERM_SWITCH(dhw_enabled);
  SUB_OPENTHERM_SWITCH(cooling_enabled);
  SUB_OPENTHERM_SWITCH(otc_active);
#endif
#ifdef USE_NUMBER
  SUB_OPENTHERM_NUMBER(ch_setpoint_temperature);
  SUB_OPENTHERM_NUMBER(ch_2_setpoint_temperature);
  SUB_OPENTHERM_NUMBER(dhw_setpoint_temperature);
#endif

 public:
  OpenThermComponent() = default;

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  static void handle_interrupt(OpenThermComponent *component);
  void set_pins(InternalGPIOPin *read_pin, InternalGPIOPin *write_pin);
  void boiler_lo_reset();
  void ch_water_filling();

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

  void request_(OpenThermMessageType type, OpenThermMessageID id, uint32_t data);
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

  uint32_t build_request_(OpenThermMessageType type, OpenThermMessageID id, uint32_t data);
  uint32_t build_response_(OpenThermMessageType type, OpenThermMessageID id, uint32_t data);
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
  uint32_t temperature_to_data_(float temperature) {
    temperature = clamp(temperature, 0.0f, 100.0f);
    return (uint32_t) (temperature * 256);
  }
};

}  // namespace opentherm
}  // namespace esphome
