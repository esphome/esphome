#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "pipsolar_switch.h"
#include "pipsolar_textsensor.h"

namespace esphome {
namespace pipsolar {

enum PollingCommand {
  POLLING_QPIRI = 0,
  POLLING_QPIGS = 1,
  POLLING_QMOD = 2,
  POLLING_QFLAG = 3,
};
struct polling_command {
  uint8_t *command;
  uint8_t length = 0;
  byte errors;
  PollingCommand identifier;
};
class PipsolarSwitch;
class Pipsolar : public uart::UARTDevice, public PollingComponent {
 public:
  void set_grid_voltage_sensor(sensor::Sensor *grid_voltage_sensor) { grid_voltage_ = grid_voltage_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_grid_frequency_sensor(sensor::Sensor *grid_frequency_sensor) { grid_frequency_ = grid_frequency_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_ac_output_voltage_sensor(sensor::Sensor *ac_output_voltage_sensor) { ac_output_voltage_ = ac_output_voltage_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_ac_output_frequency_sensor(sensor::Sensor *ac_output_frequency_sensor) { ac_output_frequency_ = ac_output_frequency_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_ac_output_apparent_power_sensor(sensor::Sensor *ac_output_apparent_power_sensor) { ac_output_apparent_power_ = ac_output_apparent_power_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_ac_output_active_power_sensor(sensor::Sensor *ac_output_active_power_sensor) { ac_output_active_power_ = ac_output_active_power_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_output_load_percent_sensor(sensor::Sensor *output_load_percent_sensor) { output_load_percent_ = output_load_percent_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_bus_voltage_sensor(sensor::Sensor *bus_voltage_sensor) { bus_voltage_ = bus_voltage_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) { battery_voltage_ = battery_voltage_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_battery_charging_current_sensor(sensor::Sensor *battery_charging_current_sensor) { battery_charging_current_ = battery_charging_current_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_battery_capacity_percent_sensor(sensor::Sensor *battery_capacity_percent_sensor) { battery_capacity_percent_ = battery_capacity_percent_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_inverter_heat_sink_temperature_sensor(sensor::Sensor *inverter_heat_sink_temperature_sensor) { inverter_heat_sink_temperature_ = inverter_heat_sink_temperature_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_pv_input_current_for_battery_sensor(sensor::Sensor *pv_input_current_for_battery_sensor) { pv_input_current_for_battery_ = pv_input_current_for_battery_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_pv_input_voltage_sensor(sensor::Sensor *pv_input_voltage_sensor) { pv_input_voltage_ = pv_input_voltage_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_battery_voltage_scc_sensor(sensor::Sensor *battery_voltage_scc_sensor) { battery_voltage_scc_ = battery_voltage_scc_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_battery_discharge_current_sensor(sensor::Sensor *battery_discharge_current_sensor) { battery_discharge_current_ = battery_discharge_current_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_add_sbu_priority_version_sensor(binary_sensor::BinarySensor *add_sbu_priority_version_sensor) { add_sbu_priority_version_ = add_sbu_priority_version_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_configuration_status_sensor(binary_sensor::BinarySensor *configuration_status_sensor) { configuration_status_ = configuration_status_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_scc_firmware_version_sensor(binary_sensor::BinarySensor *scc_firmware_version_sensor) { scc_firmware_version_ = scc_firmware_version_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_load_status_sensor(binary_sensor::BinarySensor *load_status_sensor) { load_status_ = load_status_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_battery_voltage_to_steady_while_charging_sensor(binary_sensor::BinarySensor *battery_voltage_to_steady_while_charging_sensor) { battery_voltage_to_steady_while_charging_ = battery_voltage_to_steady_while_charging_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_charging_status_sensor(binary_sensor::BinarySensor *charging_status_sensor) { charging_status_ = charging_status_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_scc_charging_status_sensor(binary_sensor::BinarySensor *scc_charging_status_sensor) { scc_charging_status_ = scc_charging_status_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_ac_charging_status_sensor(binary_sensor::BinarySensor *ac_charging_status_sensor) { ac_charging_status_ = ac_charging_status_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_battery_voltage_offset_for_fans_on_sensor(sensor::Sensor *battery_voltage_offset_for_fans_on_sensor) {battery_voltage_offset_for_fans_on_ = battery_voltage_offset_for_fans_on_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);} //.1 scale
  void set_eeprom_version_sensor(sensor::Sensor *eeprom_version_sensor) {eeprom_version_ = eeprom_version_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_pv_charging_power_sensor(sensor::Sensor *pv_charging_power_sensor) {pv_charging_power_ = pv_charging_power_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_charging_to_floating_mode_sensor(binary_sensor::BinarySensor *charging_to_floating_mode_sensor) {charging_to_floating_mode_ = charging_to_floating_mode_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_switch_on_sensor(binary_sensor::BinarySensor *switch_on_sensor) {switch_on_ = switch_on_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_dustproof_installed_sensor(binary_sensor::BinarySensor *dustproof_installed_sensor) {dustproof_installed_ = dustproof_installed_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  
  void set_grid_rating_voltage_sensor(sensor::Sensor *grid_rating_voltage_sensor) {grid_rating_voltage_ = grid_rating_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_grid_rating_current_sensor(sensor::Sensor *grid_rating_current_sensor) {grid_rating_current_ = grid_rating_current_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_ac_output_rating_voltage_sensor(sensor::Sensor *ac_output_rating_voltage_sensor) {ac_output_rating_voltage_ = ac_output_rating_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_ac_output_rating_frequency_sensor(sensor::Sensor *ac_output_rating_frequency_sensor) {ac_output_rating_frequency_ = ac_output_rating_frequency_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_ac_output_rating_current_sensor(sensor::Sensor *ac_output_rating_current_sensor) {ac_output_rating_current_ = ac_output_rating_current_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_ac_output_rating_apparent_power_sensor(sensor::Sensor *ac_output_rating_apparent_power_sensor) {ac_output_rating_apparent_power_ = ac_output_rating_apparent_power_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_ac_output_rating_active_power_sensor(sensor::Sensor *ac_output_rating_active_power_sensor) {ac_output_rating_active_power_ = ac_output_rating_active_power_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_battery_rating_voltage_sensor(sensor::Sensor *battery_rating_voltage_sensor) {battery_rating_voltage_ = battery_rating_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_battery_recharge_voltage_sensor(sensor::Sensor *battery_recharge_voltage_sensor) {battery_recharge_voltage_ = battery_recharge_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_battery_under_voltage_sensor(sensor::Sensor *battery_under_voltage_sensor) {battery_under_voltage_ = battery_under_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_battery_bulk_voltage_sensor(sensor::Sensor *battery_bulk_voltage_sensor) {battery_bulk_voltage_ = battery_bulk_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_battery_float_voltage_sensor(sensor::Sensor *battery_float_voltage_sensor) {battery_float_voltage_ = battery_float_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_battery_type_sensor(sensor::Sensor *battery_type_sensor) {battery_type_ = battery_type_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_current_max_ac_charging_current_sensor(sensor::Sensor *current_max_ac_charging_current_sensor) {current_max_ac_charging_current_ = current_max_ac_charging_current_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_current_max_charging_current_sensor(sensor::Sensor *current_max_charging_current_sensor) {current_max_charging_current_ = current_max_charging_current_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_input_voltage_range_sensor(sensor::Sensor *input_voltage_range_sensor) {input_voltage_range_ = input_voltage_range_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_output_source_priority_sensor(sensor::Sensor *output_source_priority_sensor) {output_source_priority_ = output_source_priority_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_charger_source_priority_sensor(sensor::Sensor *charger_source_priority_sensor) {charger_source_priority_ = charger_source_priority_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_parallel_max_num_sensor(sensor::Sensor *parallel_max_num_sensor) {parallel_max_num_ = parallel_max_num_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_machine_type_sensor(sensor::Sensor *machine_type_sensor) {machine_type_ = machine_type_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_topology_sensor(sensor::Sensor *topology_sensor) {topology_ = topology_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_output_mode_sensor(sensor::Sensor *output_mode_sensor) {output_mode_ = output_mode_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_battery_redischarge_voltage_sensor(sensor::Sensor *battery_redischarge_voltage_sensor) {battery_redischarge_voltage_ = battery_redischarge_voltage_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_pv_ok_condition_for_parallel_sensor(sensor::Sensor *pv_ok_condition_for_parallel_sensor) {pv_ok_condition_for_parallel_ = pv_ok_condition_for_parallel_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_pv_power_balance_sensor(sensor::Sensor *pv_power_balance_sensor) {pv_power_balance_ = pv_power_balance_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}

  void set_device_mode_sensor(text_sensor::TextSensor *device_mode_sensor) {device_mode_ = device_mode_sensor; this->add_polling_command("QMOD",POLLING_QMOD);}

  void set_silence_buzzer_open_buzzer_sensor(binary_sensor::BinarySensor *silence_buzzer_open_buzzer_sensor) {silence_buzzer_open_buzzer_ = silence_buzzer_open_buzzer_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}
  void set_overload_bypass_function_sensor(binary_sensor::BinarySensor *overload_bypass_function_sensor) {overload_bypass_function_ = overload_bypass_function_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}
  void set_lcd_escape_to_default_sensor(binary_sensor::BinarySensor *lcd_escape_to_default_sensor) {lcd_escape_to_default_ = lcd_escape_to_default_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}
  void set_overload_restart_function_sensor(binary_sensor::BinarySensor *overload_restart_function_sensor) {overload_restart_function_ = overload_restart_function_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}
  void set_over_temperature_restart_function_sensor(binary_sensor::BinarySensor *over_temperature_restart_function_sensor) {over_temperature_restart_function_ = over_temperature_restart_function_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}
  void set_backlight_on_sensor(binary_sensor::BinarySensor *backlight_on_sensor) {backlight_on_ = backlight_on_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}
  void set_alarm_on_when_primary_source_interrupt_sensor(binary_sensor::BinarySensor *alarm_on_when_primary_source_interrupt_sensor) {alarm_on_when_primary_source_interrupt_ = alarm_on_when_primary_source_interrupt_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}
  void set_fault_code_record_sensor(binary_sensor::BinarySensor *fault_code_record_sensor) {fault_code_record_ = fault_code_record_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}


  void set_last_qpigs_sensor(text_sensor::TextSensor *last_qpigs_sensor) {last_qpigs_ = last_qpigs_sensor; this->add_polling_command("QPIGS",POLLING_QPIGS);}
  void set_last_qpiri_sensor(text_sensor::TextSensor *last_qpiri_sensor) {last_qpiri_ = last_qpiri_sensor; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_last_qmod_sensor(text_sensor::TextSensor *last_qmod_sensor) {last_qmod_ = last_qmod_sensor; this->add_polling_command("QMOD",POLLING_QMOD);}
  void set_last_qflag_sensor(text_sensor::TextSensor *last_qflag_sensor) {last_qflag_ = last_qflag_sensor; this->add_polling_command("QFLAG",POLLING_QFLAG);}

  void set_output_source_priority_utility_switch(PipsolarSwitch *output_source_priority_utility_switch) { output_source_priority_utility_switch_ = output_source_priority_utility_switch; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_output_source_priority_solar_switch(PipsolarSwitch *output_source_priority_solar_switch) { output_source_priority_solar_switch_ = output_source_priority_solar_switch; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_output_source_priority_battery_switch(PipsolarSwitch *output_source_priority_battery_switch) { output_source_priority_battery_switch_ = output_source_priority_battery_switch; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_input_voltage_range_switch(PipsolarSwitch *input_voltage_range_switch) { input_voltage_range_switch_ = input_voltage_range_switch; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_pv_ok_condition_for_parallel_switch(PipsolarSwitch *pv_ok_condition_for_parallel_switch) { pv_ok_condition_for_parallel_switch_ = pv_ok_condition_for_parallel_switch; this->add_polling_command("QPIRI",POLLING_QPIRI);}
  void set_pv_power_balance_switch(PipsolarSwitch *pv_power_balance_switch) { pv_power_balance_switch_ = pv_power_balance_switch; this->add_polling_command("QPIRI",POLLING_QPIRI);}

  void switch_command(String source);
  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;

 protected:
  static const size_t PIPSOLAR_READ_BUFFER_LENGTH = 110; //maximum supported answer length
  static const size_t COMMAND_QUEUE_LENGTH = 10; 
  static const size_t COMMAND_TIMEOUT = 5000;
  void add_polling_command(const char* command, PollingCommand polling_command);
  void empty_uart_buffer();
  uint8_t check_incoming_crc();
  uint8_t check_incoming_length(uint8_t length);
  uint16_t calc_crc(uint8_t *msg,int n);
  uint16_t crc_xmodem_update (uint16_t crc, uint8_t data);
  uint8_t send_next_command();
  void send_next_poll();
  void queue_command(const char *command, byte length);
  String command_queue_[COMMAND_QUEUE_LENGTH];
  byte command_queue_position_ = 0;
  uint8_t read_buffer_[PIPSOLAR_READ_BUFFER_LENGTH];
  size_t read_pos_{0};
  bool update_running = 0;

  PipsolarSwitch *output_source_priority_utility_switch_;
  PipsolarSwitch *output_source_priority_solar_switch_;
  PipsolarSwitch *output_source_priority_battery_switch_;
  PipsolarSwitch *input_voltage_range_switch_; //00 for appliance, 01 for UPS
  PipsolarSwitch *pv_ok_condition_for_parallel_switch_;
  PipsolarSwitch *pv_power_balance_switch_;


  text_sensor::TextSensor *last_qpigs_;
  text_sensor::TextSensor *last_qpiri_;
  text_sensor::TextSensor *last_qmod_;
  text_sensor::TextSensor *last_qflag_;
//QPIRI Sensors
  sensor::Sensor *grid_rating_voltage_;
  sensor::Sensor *grid_rating_current_;
  sensor::Sensor *ac_output_rating_voltage_;
  sensor::Sensor *ac_output_rating_frequency_;
  sensor::Sensor *ac_output_rating_current_;
  sensor::Sensor *ac_output_rating_apparent_power_;
  sensor::Sensor *ac_output_rating_active_power_;
  sensor::Sensor *battery_rating_voltage_;
  sensor::Sensor *battery_recharge_voltage_;
  sensor::Sensor *battery_under_voltage_;
  sensor::Sensor *battery_bulk_voltage_;
  sensor::Sensor *battery_float_voltage_;
  sensor::Sensor *battery_type_;
  sensor::Sensor *current_max_ac_charging_current_;
  sensor::Sensor *current_max_charging_current_;
  sensor::Sensor *input_voltage_range_;
  sensor::Sensor *output_source_priority_;
  sensor::Sensor *charger_source_priority_;
  sensor::Sensor *parallel_max_num_;
  sensor::Sensor *machine_type_;
  sensor::Sensor *topology_;
  sensor::Sensor *output_mode_;
  sensor::Sensor *battery_redischarge_voltage_;
  sensor::Sensor *pv_ok_condition_for_parallel_;
  sensor::Sensor *pv_power_balance_;
//QPIGS Sensors
  sensor::Sensor *grid_voltage_;
  sensor::Sensor *grid_frequency_;
  sensor::Sensor *ac_output_voltage_;
  sensor::Sensor *ac_output_frequency_;
  sensor::Sensor *ac_output_apparent_power_;
  sensor::Sensor *ac_output_active_power_;
  sensor::Sensor *output_load_percent_;
  sensor::Sensor *bus_voltage_;
  sensor::Sensor *battery_voltage_;
  sensor::Sensor *battery_charging_current_;
  sensor::Sensor *battery_capacity_percent_;
  sensor::Sensor *inverter_heat_sink_temperature_;
  sensor::Sensor *pv_input_current_for_battery_;
  sensor::Sensor *pv_input_voltage_;
  sensor::Sensor *battery_voltage_scc_;
  sensor::Sensor *battery_discharge_current_;
  binary_sensor::BinarySensor *add_sbu_priority_version_;
  binary_sensor::BinarySensor *configuration_status_;
  binary_sensor::BinarySensor *scc_firmware_version_;
  binary_sensor::BinarySensor *load_status_;
  binary_sensor::BinarySensor *battery_voltage_to_steady_while_charging_;
  binary_sensor::BinarySensor *charging_status_;
  binary_sensor::BinarySensor *scc_charging_status_;
  binary_sensor::BinarySensor *ac_charging_status_;

  sensor::Sensor *battery_voltage_offset_for_fans_on_; 
  sensor::Sensor *eeprom_version_;
  sensor::Sensor *pv_charging_power_;
  binary_sensor::BinarySensor *charging_to_floating_mode_;
  binary_sensor::BinarySensor *switch_on_;
  binary_sensor::BinarySensor *dustproof_installed_;
//QMOD Sensors
  text_sensor::TextSensor *device_mode_;
  binary_sensor::BinarySensor *device_mode_power_on_;
  binary_sensor::BinarySensor *device_mode_standby_;
  binary_sensor::BinarySensor *device_mode_line_;
  binary_sensor::BinarySensor *device_mode_battery_;
  binary_sensor::BinarySensor *device_mode_fault_;
  binary_sensor::BinarySensor *device_mode_power_saving_;

//QFLAG Sensors
  binary_sensor::BinarySensor *silence_buzzer_open_buzzer_;
  binary_sensor::BinarySensor *overload_bypass_function_;
  binary_sensor::BinarySensor *lcd_escape_to_default_;
  binary_sensor::BinarySensor *overload_restart_function_;
  binary_sensor::BinarySensor *over_temperature_restart_function_;
  binary_sensor::BinarySensor *backlight_on_;
  binary_sensor::BinarySensor *alarm_on_when_primary_source_interrupt_;
  binary_sensor::BinarySensor *fault_code_record_;



//QPIRI values
  float grid_rating_voltage;
  float grid_rating_current;
  float ac_output_rating_voltage;
  float ac_output_rating_frequency;
  float ac_output_rating_current;
  int ac_output_rating_apparent_power;
  int ac_output_rating_active_power;
  float battery_rating_voltage;
  float battery_recharge_voltage;
  float battery_under_voltage;
  float battery_bulk_voltage;
  float battery_float_voltage;
  int battery_type;
  int current_max_ac_charging_current;
  int current_max_charging_current;
  int input_voltage_range;
  int output_source_priority;
  int charger_source_priority;
  int parallel_max_num;
  int machine_type;
  int topology;
  int output_mode;
  float battery_redischarge_voltage;
  int pv_ok_condition_for_parallel;
  int pv_power_balance;
//QPIGS values
  float grid_voltage;
  float grid_frequency;
  float ac_output_voltage;
  float ac_output_frequency;
  int ac_output_apparent_power;
  int ac_output_active_power;
  int output_load_percent;
  int bus_voltage;
  float battery_voltage;
  int battery_charging_current;
  int battery_capacity_percent;
  int inverter_heat_sink_temperature;
  int pv_input_current_for_battery;
  float pv_input_voltage;
  float battery_voltage_scc;
  int battery_discharge_current;
  int add_sbu_priority_version;
  int configuration_status;
  int scc_firmware_version;
  int load_status;
  int battery_voltage_to_steady_while_charging;
  int charging_status;
  int scc_charging_status;
  int ac_charging_status;

  int battery_voltage_offset_for_fans_on; //.1 scale
  int eeprom_version;
  int pv_charging_power;
  int charging_to_floating_mode;
  int switch_on;
  int dustproof_installed;

//QMOD values
  char device_mode;

//QFLAG values
  int silence_buzzer_open_buzzer;
  int overload_bypass_function;
  int lcd_escape_to_default;
  int overload_restart_function;
  int over_temperature_restart_function;
  int backlight_on;
  int alarm_on_when_primary_source_interrupt;
  int fault_code_record;

  
  uint32_t command_start_millis_ = 0;
  uint8_t state_;
  enum State {
    STATE_IDLE = 0,
    STATE_POLL = 1,
    STATE_COMMAND = 2,
    STATE_POLL_COMPLETE = 3,
    STATE_COMMAND_COMPLETE = 4,
    STATE_POLL_CHECKED = 5,
    STATE_POLL_DECODED = 6,
  };

  uint8_t last_polling_command = 0;
  polling_command used_polling_commands_[15];

};

}  // namespace pipsolar
}  // namespace esphome

