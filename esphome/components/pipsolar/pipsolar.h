#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pipsolar {

enum ENUMPollingCommand {
  POLLING_QPIRI = 0,
  POLLING_QPIGS = 1,
  POLLING_QMOD = 2,
  POLLING_QFLAG = 3,
  POLLING_QPIWS = 4,
  POLLING_QT = 5,
  POLLING_QMN = 6,
};
struct PollingCommand {
  uint8_t *command;
  uint8_t length = 0;
  uint8_t errors;
  ENUMPollingCommand identifier;
  bool needs_update;
};

struct QPIRIValues {
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
};
struct QPIGSValues {
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
  float pv_input_current_for_battery;
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
  int battery_voltage_offset_for_fans_on;
  int eeprom_version;
  int pv_charging_power;
  int charging_to_floating_mode;
  int switch_on;
  int dustproof_installed;
};
struct QFLAGValues {
  int silence_buzzer_open_buzzer;
  int overload_bypass_function;
  int lcd_escape_to_default;
  int overload_restart_function;
  int over_temperature_restart_function;
  int backlight_on;
  int alarm_on_when_primary_source_interrupt;
  int fault_code_record;
  int power_saving;
};
struct QPIWSValues {
  bool warnings_present;
  bool faults_present;
  bool warning_power_loss;
  bool fault_inverter_fault;
  bool fault_bus_over;
  bool fault_bus_under;
  bool fault_bus_soft_fail;
  bool warning_line_fail;
  bool fault_opvshort;
  bool fault_inverter_voltage_too_low;
  bool fault_inverter_voltage_too_high;
  bool warning_over_temperature;
  bool warning_fan_lock;
  bool warning_battery_voltage_high;
  bool warning_battery_low_alarm;
  bool warning_battery_under_shutdown;
  bool warning_battery_derating;
  bool warning_over_load;
  bool warning_eeprom_failed;
  bool fault_inverter_over_current;
  bool fault_inverter_soft_failed;
  bool fault_self_test_failed;
  bool fault_op_dc_voltage_over;
  bool fault_battery_open;
  bool fault_current_sensor_failed;
  bool fault_battery_short;
  bool warning_power_limit;
  bool warning_pv_voltage_high;
  bool fault_mppt_overload;
  bool warning_mppt_overload;
  bool warning_battery_too_low_to_charge;
  bool fault_dc_dc_over_current;
  int fault_code;
  bool warnung_low_pv_energy;
  bool warning_high_ac_input_during_bus_soft_start;
  bool warning_battery_equalization;
};


#define PIPSOLAR_ENTITY_(type, name, polling_command) \
 protected: \
  type *name##_{}; /* NOLINT */ \
\
 public: \
  void set_##name(type *name) { /* NOLINT */ \
    this->name##_ = name; \
    this->add_polling_command_(#polling_command, POLLING_##polling_command); \
  }

#define PIPSOLAR_SENSOR(name, polling_command) \
  PIPSOLAR_ENTITY_(sensor::Sensor, name, polling_command)
#define PIPSOLAR_SWITCH(name, polling_command) PIPSOLAR_ENTITY_(switch_::Switch, name, polling_command)
#define PIPSOLAR_BINARY_SENSOR(name, polling_command) \
  PIPSOLAR_ENTITY_(binary_sensor::BinarySensor, name, polling_command)
#define PIPSOLAR_TEXT_SENSOR(name, polling_command) PIPSOLAR_ENTITY_(text_sensor::TextSensor, name, polling_command)

class Pipsolar : public uart::UARTDevice, public PollingComponent {
  // QPIGS values
  PIPSOLAR_SENSOR(grid_voltage, QPIGS)
  PIPSOLAR_SENSOR(grid_frequency, QPIGS)
  PIPSOLAR_SENSOR(ac_output_voltage, QPIGS)
  PIPSOLAR_SENSOR(ac_output_frequency, QPIGS)
  PIPSOLAR_SENSOR(ac_output_apparent_power, QPIGS)
  PIPSOLAR_SENSOR(ac_output_active_power, QPIGS)
  PIPSOLAR_SENSOR(output_load_percent, QPIGS)
  PIPSOLAR_SENSOR(bus_voltage, QPIGS)
  PIPSOLAR_SENSOR(battery_voltage, QPIGS)
  PIPSOLAR_SENSOR(battery_charging_current, QPIGS)
  PIPSOLAR_SENSOR(battery_capacity_percent, QPIGS)
  PIPSOLAR_SENSOR(inverter_heat_sink_temperature, QPIGS)
  PIPSOLAR_SENSOR(pv_input_current_for_battery, QPIGS)
  PIPSOLAR_SENSOR(pv_input_voltage, QPIGS)
  PIPSOLAR_SENSOR(battery_voltage_scc, QPIGS)
  PIPSOLAR_SENSOR(battery_discharge_current, QPIGS)
  PIPSOLAR_BINARY_SENSOR(add_sbu_priority_version, QPIGS)
  PIPSOLAR_BINARY_SENSOR(configuration_status, QPIGS)
  PIPSOLAR_BINARY_SENSOR(scc_firmware_version, QPIGS)
  PIPSOLAR_BINARY_SENSOR(load_status, QPIGS)
  PIPSOLAR_BINARY_SENSOR(battery_voltage_to_steady_while_charging, QPIGS)
  PIPSOLAR_BINARY_SENSOR(charging_status, QPIGS)
  PIPSOLAR_BINARY_SENSOR(scc_charging_status, QPIGS)
  PIPSOLAR_BINARY_SENSOR(ac_charging_status, QPIGS)
  PIPSOLAR_SENSOR(battery_voltage_offset_for_fans_on, QPIGS)  //.1 scale
  PIPSOLAR_SENSOR(eeprom_version, QPIGS)
  PIPSOLAR_SENSOR(pv_charging_power, QPIGS)
  PIPSOLAR_BINARY_SENSOR(charging_to_floating_mode, QPIGS)
  PIPSOLAR_BINARY_SENSOR(switch_on, QPIGS)
  PIPSOLAR_BINARY_SENSOR(dustproof_installed, QPIGS)

  // QPIRI values
  PIPSOLAR_SENSOR(grid_rating_voltage, QPIRI)
  PIPSOLAR_SENSOR(grid_rating_current, QPIRI)
  PIPSOLAR_SENSOR(ac_output_rating_voltage, QPIRI)
  PIPSOLAR_SENSOR(ac_output_rating_frequency, QPIRI)
  PIPSOLAR_SENSOR(ac_output_rating_current, QPIRI)
  PIPSOLAR_SENSOR(ac_output_rating_apparent_power, QPIRI)
  PIPSOLAR_SENSOR(ac_output_rating_active_power, QPIRI)
  PIPSOLAR_SENSOR(battery_rating_voltage, QPIRI)
  PIPSOLAR_SENSOR(battery_recharge_voltage, QPIRI)
  PIPSOLAR_SENSOR(battery_under_voltage, QPIRI)
  PIPSOLAR_SENSOR(battery_bulk_voltage, QPIRI)
  PIPSOLAR_SENSOR(battery_float_voltage, QPIRI)
  PIPSOLAR_SENSOR(battery_type, QPIRI)
  PIPSOLAR_SENSOR(current_max_ac_charging_current, QPIRI)
  PIPSOLAR_SENSOR(current_max_charging_current, QPIRI)
  PIPSOLAR_SENSOR(input_voltage_range, QPIRI)
  PIPSOLAR_SENSOR(output_source_priority, QPIRI)
  PIPSOLAR_SENSOR(charger_source_priority, QPIRI)
  PIPSOLAR_SENSOR(parallel_max_num, QPIRI)
  PIPSOLAR_SENSOR(machine_type, QPIRI)
  PIPSOLAR_SENSOR(topology, QPIRI)
  PIPSOLAR_SENSOR(output_mode, QPIRI)
  PIPSOLAR_SENSOR(battery_redischarge_voltage, QPIRI)
  PIPSOLAR_SENSOR(pv_ok_condition_for_parallel, QPIRI)
  PIPSOLAR_SENSOR(pv_power_balance, QPIRI)

  // QMOD values
  PIPSOLAR_TEXT_SENSOR(device_mode, QMOD)

  // QFLAG values
  PIPSOLAR_BINARY_SENSOR(silence_buzzer_open_buzzer, QFLAG)
  PIPSOLAR_BINARY_SENSOR(overload_bypass_function, QFLAG)
  PIPSOLAR_BINARY_SENSOR(lcd_escape_to_default, QFLAG)
  PIPSOLAR_BINARY_SENSOR(overload_restart_function, QFLAG)
  PIPSOLAR_BINARY_SENSOR(over_temperature_restart_function, QFLAG)
  PIPSOLAR_BINARY_SENSOR(backlight_on, QFLAG)
  PIPSOLAR_BINARY_SENSOR(alarm_on_when_primary_source_interrupt, QFLAG)
  PIPSOLAR_BINARY_SENSOR(fault_code_record, QFLAG)
  PIPSOLAR_BINARY_SENSOR(power_saving, QFLAG)

  // QPIWS values
  PIPSOLAR_BINARY_SENSOR(warnings_present, QPIWS)
  PIPSOLAR_BINARY_SENSOR(faults_present, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_power_loss, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_inverter_fault, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_bus_over, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_bus_under, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_bus_soft_fail, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_line_fail, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_opvshort, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_inverter_voltage_too_low, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_inverter_voltage_too_high, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_over_temperature, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_fan_lock, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_battery_voltage_high, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_battery_low_alarm, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_battery_under_shutdown, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_battery_derating, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_over_load, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_eeprom_failed, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_inverter_over_current, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_inverter_soft_failed, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_self_test_failed, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_op_dc_voltage_over, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_battery_open, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_current_sensor_failed, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_battery_short, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_power_limit, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_pv_voltage_high, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_mppt_overload, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_mppt_overload, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_battery_too_low_to_charge, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_dc_dc_over_current, QPIWS)
  PIPSOLAR_BINARY_SENSOR(fault_code, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warnung_low_pv_energy, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_high_ac_input_during_bus_soft_start, QPIWS)
  PIPSOLAR_BINARY_SENSOR(warning_battery_equalization, QPIWS)

  PIPSOLAR_TEXT_SENSOR(last_qpigs, QPIGS)
  PIPSOLAR_TEXT_SENSOR(last_qpiri, QPIRI)
  PIPSOLAR_TEXT_SENSOR(last_qmod, QMOD)
  PIPSOLAR_TEXT_SENSOR(last_qflag, QFLAG)
  PIPSOLAR_TEXT_SENSOR(last_qpiws, QPIWS)
  PIPSOLAR_TEXT_SENSOR(last_qt, QT)
  PIPSOLAR_TEXT_SENSOR(last_qmn, QMN)

  PIPSOLAR_SWITCH(output_source_priority_utility_switch, QPIRI)
  PIPSOLAR_SWITCH(output_source_priority_solar_switch, QPIRI)
  PIPSOLAR_SWITCH(output_source_priority_battery_switch, QPIRI)
  PIPSOLAR_SWITCH(output_source_priority_hybrid_switch, QPIRI)
  PIPSOLAR_SWITCH(input_voltage_range_switch, QPIRI)
  PIPSOLAR_SWITCH(pv_ok_condition_for_parallel_switch, QPIRI)
  PIPSOLAR_SWITCH(pv_power_balance_switch, QPIRI)

  void queue_command(const std::string &command);
  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;

 protected:
  static const size_t PIPSOLAR_READ_BUFFER_LENGTH = 110;  // maximum supported answer length
  static const size_t COMMAND_QUEUE_LENGTH = 10;
  static const size_t COMMAND_TIMEOUT = 5000;
  static const size_t POLLING_COMMANDS_MAX = 15;
  void add_polling_command_(const char *command, ENUMPollingCommand polling_command);
  void empty_uart_buffer_();
  uint8_t check_incoming_crc_();
  uint8_t check_incoming_length_(uint8_t length);
  uint16_t pipsolar_crc_(uint8_t *msg, uint8_t len);
  bool send_next_command_();
  bool send_next_poll_();

  void handle_qpiri_(const char *message);
  void handle_qpigs_(const char *message);
  void handle_qmod_(const char *message);
  void handle_qflag_(const char *message);
  void handle_qpiws_(const char *message);
  void handle_qt_(const char *message);
  void handle_qmn_(const char *message);

  std::string command_queue_[COMMAND_QUEUE_LENGTH];
  uint8_t command_queue_position_ = 0;
  uint8_t read_buffer_[PIPSOLAR_READ_BUFFER_LENGTH];
  size_t read_pos_{0};

  uint32_t command_start_millis_ = 0;
  uint8_t state_;
  enum State {
    STATE_IDLE = 0,
    STATE_POLL = 1,
    STATE_COMMAND = 2,
    STATE_POLL_COMPLETE = 3,
    STATE_COMMAND_COMPLETE = 4,
    STATE_POLL_CHECKED = 5,
  };

  uint8_t last_polling_command_ = 0;
  PollingCommand enabled_polling_commands_[POLLING_COMMANDS_MAX];
};

}  // namespace pipsolar
}  // namespace esphome
