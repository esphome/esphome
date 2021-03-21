#include "pipsolar.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pipsolar {

static const char* TAG = "pipsolar";

void Pipsolar::setup() {
  this->state_ = STATE_IDLE;
  this->command_start_millis_ = 0;
}

void Pipsolar::empty_uart_buffer() {
  uint8_t byte;
  while (this->available()) {
    this->read_byte(&byte);
  }
}

void Pipsolar::loop() {
  // Read message
  if (this->state_ == STATE_IDLE) {
    this->empty_uart_buffer();
    switch (this->send_next_command()) {
      case 0:
        // no command send (empty queue) time to poll
        if (millis() - this->last_poll > this->update_interval_) {
          this->send_next_poll();
          this->last_poll = millis();
        }
        return;
        break;
      case 1: 
        // command send
        return;
        break;
    }
  }
  if (this->state_ == STATE_COMMAND_COMPLETE ) {
    if (this->check_incoming_length(3)) {
      ESP_LOGD(TAG,"response length for command OK");
      if (this->check_incoming_crc()) {
        //crc ok
        if (this->read_buffer_[0] == 'A' && this->read_buffer_[1] == 'C' && this->read_buffer_[2] == 'K') {
          ESP_LOGD(TAG,"command successful");
        } else {
          ESP_LOGD(TAG,"command not successful");
        }
        this->command_queue_[this->command_queue_position_] = String("");
        this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
        this->state_ = STATE_IDLE;

      } else {
        //crc failed
        this->command_queue_[this->command_queue_position_] = String("");
        this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
        this->state_ = STATE_IDLE;
      }
    } else {
      this->command_queue_[this->command_queue_position_] = String("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
      ESP_LOGD(TAG,"response length for command not OK: with length %d",this->read_pos_);

    }
  }

  if (this->state_ == STATE_POLL_DECODED) {
    switch(this->used_polling_commands_[this->last_polling_command].identifier) {
      case POLLING_QPIRI:
        if (this->grid_rating_voltage_) {this->grid_rating_voltage_->publish_state(grid_rating_voltage);}
        if (this->grid_rating_current_) {this->grid_rating_current_->publish_state(grid_rating_current);}
        if (this->ac_output_rating_voltage_) {this->ac_output_rating_voltage_->publish_state(ac_output_rating_voltage);}
        if (this->ac_output_rating_frequency_) {this->ac_output_rating_frequency_->publish_state(ac_output_rating_frequency);}
        if (this->ac_output_rating_current_) {this->ac_output_rating_current_->publish_state(ac_output_rating_current);}
        if (this->ac_output_rating_apparent_power_) {this->ac_output_rating_apparent_power_->publish_state(ac_output_rating_apparent_power);}
        if (this->ac_output_rating_active_power_) {this->ac_output_rating_active_power_->publish_state(ac_output_rating_active_power);}
        if (this->battery_rating_voltage_) {this->battery_rating_voltage_->publish_state(battery_rating_voltage);}
        if (this->battery_recharge_voltage_) {this->battery_recharge_voltage_->publish_state(battery_recharge_voltage);}
        if (this->battery_under_voltage_) {this->battery_under_voltage_->publish_state(battery_under_voltage);}
        if (this->battery_bulk_voltage_) {this->battery_bulk_voltage_->publish_state(battery_bulk_voltage);}
        if (this->battery_float_voltage_) {this->battery_float_voltage_->publish_state(battery_float_voltage);}
        if (this->battery_type_) {this->battery_type_->publish_state(battery_type);}
        if (this->current_max_ac_charging_current_) {this->current_max_ac_charging_current_->publish_state(current_max_ac_charging_current);}
        if (this->current_max_charging_current_) {this->current_max_charging_current_->publish_state(current_max_charging_current);}
        if (this->input_voltage_range_) {this->input_voltage_range_->publish_state(input_voltage_range);}
        // special for input voltage range switch
        if (this->input_voltage_range_switch_) {this->input_voltage_range_switch_->publish_state(this->input_voltage_range_);}
        if (this->output_source_priority_) {this->output_source_priority_->publish_state(output_source_priority);}
        // special for output source priority switches
        if (this->output_source_priority_utility_switch_) {this->output_source_priority_utility_switch_->publish_state(output_source_priority == 0);}
        if (this->output_source_priority_solar_switch_) {this->output_source_priority_solar_switch_->publish_state(output_source_priority == 1);}
        if (this->output_source_priority_battery_switch_) {this->output_source_priority_battery_switch_->publish_state(output_source_priority == 2);}
        if (this->charger_source_priority_) {this->charger_source_priority_->publish_state(charger_source_priority);}
        if (this->parallel_max_num_) {this->parallel_max_num_->publish_state(parallel_max_num);}
        if (this->machine_type_) {this->machine_type_->publish_state(machine_type);}
        if (this->topology_) {this->topology_->publish_state(topology);}
        if (this->output_mode_) {this->output_mode_->publish_state(output_mode);}
        if (this->battery_redischarge_voltage_) {this->battery_redischarge_voltage_->publish_state(battery_redischarge_voltage);}
        if (this->pv_ok_condition_for_parallel_) {this->pv_ok_condition_for_parallel_->publish_state(pv_ok_condition_for_parallel);}
        // special for pv ok condition switch
        if (this->pv_ok_condition_for_parallel_switch_) {this->pv_ok_condition_for_parallel_switch_->publish_state(this->pv_ok_condition_for_parallel_);}
        if (this->pv_power_balance_) {this->pv_power_balance_->publish_state(pv_power_balance);}
        // special for power balance switch
        if (this->pv_power_balance_switch_) {this->pv_power_balance_switch_->publish_state(this->pv_power_balance_);}


        this->state_ = STATE_IDLE;
        break;
      case POLLING_QPIGS:
        if (this->grid_voltage_) {this->grid_voltage_->publish_state(grid_voltage);}
        if (this->grid_frequency_) {this->grid_frequency_->publish_state(grid_frequency);}
        if (this->ac_output_voltage_) {this->ac_output_voltage_->publish_state(ac_output_voltage);}
        if (this->ac_output_frequency_) {this->ac_output_frequency_->publish_state(ac_output_frequency);}
        if (this->ac_output_apparent_power_) {this->ac_output_apparent_power_->publish_state(ac_output_apparent_power);}
        if (this->ac_output_active_power_) {this->ac_output_active_power_->publish_state(ac_output_active_power);}
        if (this->output_load_percent_) {this->output_load_percent_->publish_state(output_load_percent);}
        if (this->bus_voltage_) {this->bus_voltage_->publish_state(bus_voltage);}
        if (this->battery_voltage_) {this->battery_voltage_->publish_state(battery_voltage);}
        if (this->battery_charging_current_) {this->battery_charging_current_->publish_state(battery_charging_current);}
        if (this->battery_capacity_percent_) {this->battery_capacity_percent_->publish_state(battery_capacity_percent);}
        if (this->inverter_heat_sink_temperature_) {this->inverter_heat_sink_temperature_->publish_state(inverter_heat_sink_temperature);}
        if (this->pv_input_current_for_battery_) {this->pv_input_current_for_battery_->publish_state(pv_input_current_for_battery);}
        if (this->pv_input_voltage_) {this->pv_input_voltage_->publish_state(pv_input_voltage);}
        if (this->battery_voltage_scc_) {this->battery_voltage_scc_->publish_state(battery_voltage_scc);}
        if (this->battery_discharge_current_) {this->battery_discharge_current_->publish_state(battery_discharge_current);}
        if (this->add_sbu_priority_version_) {this->add_sbu_priority_version_->publish_state(add_sbu_priority_version);}
        if (this->configuration_status_) {this->configuration_status_->publish_state(configuration_status);}
        if (this->scc_firmware_version_) {this->scc_firmware_version_->publish_state(scc_firmware_version);}
        if (this->load_status_) {this->load_status_->publish_state(load_status);}
        if (this->battery_voltage_to_steady_while_charging_) {this->battery_voltage_to_steady_while_charging_->publish_state(battery_voltage_to_steady_while_charging);}
        if (this->charging_status_) {this->charging_status_->publish_state(charging_status);}
        if (this->scc_charging_status_) {this->scc_charging_status_->publish_state(scc_charging_status);}
        if (this->ac_charging_status_) {this->ac_charging_status_->publish_state(ac_charging_status);}
        if (this->battery_voltage_offset_for_fans_on_) {this->battery_voltage_offset_for_fans_on_->publish_state(battery_voltage_offset_for_fans_on/10.0f);} //.1 scale
        if (this->eeprom_version_) {this->eeprom_version_->publish_state(eeprom_version);}
        if (this->pv_charging_power_) {this->pv_charging_power_->publish_state(pv_charging_power);}
        if (this->charging_to_floating_mode_) {this->charging_to_floating_mode_->publish_state(charging_to_floating_mode);}
        if (this->switch_on_) {this->switch_on_->publish_state(switch_on);}
        if (this->dustproof_installed_) {this->dustproof_installed_->publish_state(dustproof_installed);}
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QMOD:
        if (this->device_mode_) {this->device_mode_->publish_state(String(device_mode).c_str());}
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QFLAG:
        if (this->silence_buzzer_open_buzzer_) {this->silence_buzzer_open_buzzer_->publish_state(this->silence_buzzer_open_buzzer);}
        if (this->overload_bypass_function_) {this->overload_bypass_function_->publish_state(this->overload_bypass_function);}
        if (this->lcd_escape_to_default_) {this->lcd_escape_to_default_->publish_state(this->lcd_escape_to_default);}
        if (this->overload_restart_function_) {this->overload_restart_function_->publish_state(this->overload_restart_function);}
        if (this->over_temperature_restart_function_) {this->over_temperature_restart_function_->publish_state(this->over_temperature_restart_function);}
        if (this->backlight_on_) {this->backlight_on_->publish_state(this->backlight_on);}
        if (this->alarm_on_when_primary_source_interrupt_) {this->alarm_on_when_primary_source_interrupt_->publish_state(this->alarm_on_when_primary_source_interrupt);}
        if (this->fault_code_record_) {this->fault_code_record_->publish_state(this->fault_code_record);}
        if (this->power_saving_) {this->power_saving_->publish_state(this->power_saving);}
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QPIWS:
        if (this->warnings_present_) {this->warnings_present_->publish_state(this->warnings_present);}
        if (this->faults_present_) {this->faults_present_->publish_state(this->faults_present);}
        if (this->warning_power_loss_) {this->warning_power_loss_->publish_state(this->warning_power_loss);}
        if (this->fault_inverter_fault_) {this->fault_inverter_fault_->publish_state(this->fault_inverter_fault);}
        if (this->fault_bus_over_) {this->fault_bus_over_->publish_state(this->fault_bus_over);}
        if (this->fault_bus_under_) {this->fault_bus_under_->publish_state(this->fault_bus_under);}
        if (this->fault_bus_soft_fail_) {this->fault_bus_soft_fail_->publish_state(this->fault_bus_soft_fail);}
        if (this->warning_line_fail_) {this->warning_line_fail_->publish_state(this->warning_line_fail);}
        if (this->fault_opvshort_) {this->fault_opvshort_->publish_state(this->fault_opvshort);}
        if (this->fault_inverter_voltage_too_low_) {this->fault_inverter_voltage_too_low_->publish_state(this->fault_inverter_voltage_too_low);}
        if (this->fault_inverter_voltage_too_high_) {this->fault_inverter_voltage_too_high_->publish_state(this->fault_inverter_voltage_too_high);}
        if (this->warning_over_temperature_) {this->warning_over_temperature_->publish_state(this->warning_over_temperature);}
        if (this->warning_fan_lock_) {this->warning_fan_lock_->publish_state(this->warning_fan_lock);}
        if (this->warning_battery_voltage_high_) {this->warning_battery_voltage_high_->publish_state(this->warning_battery_voltage_high);}
        if (this->warning_battery_low_alarm_) {this->warning_battery_low_alarm_->publish_state(this->warning_battery_low_alarm);}
        if (this->warning_battery_under_shutdown_) {this->warning_battery_under_shutdown_->publish_state(this->warning_battery_under_shutdown);}
        if (this->warning_battery_derating_) {this->warning_battery_derating_->publish_state(this->warning_battery_derating);}
        if (this->warning_over_load_) {this->warning_over_load_->publish_state(this->warning_over_load);}
        if (this->warning_eeprom_failed_) {this->warning_eeprom_failed_->publish_state(this->warning_eeprom_failed);}
        if (this->fault_inverter_over_current_) {this->fault_inverter_over_current_->publish_state(this->fault_inverter_over_current);}
        if (this->fault_inverter_soft_failed_) {this->fault_inverter_soft_failed_->publish_state(this->fault_inverter_soft_failed);}
        if (this->fault_self_test_failed_) {this->fault_self_test_failed_->publish_state(this->fault_self_test_failed);}
        if (this->fault_op_dc_voltage_over_) {this->fault_op_dc_voltage_over_->publish_state(this->fault_op_dc_voltage_over);}
        if (this->fault_battery_open_) {this->fault_battery_open_->publish_state(this->fault_battery_open);}
        if (this->fault_current_sensor_failed_) {this->fault_current_sensor_failed_->publish_state(this->fault_current_sensor_failed);}
        if (this->fault_battery_short_) {this->fault_battery_short_->publish_state(this->fault_battery_short);}
        if (this->warning_power_limit_) {this->warning_power_limit_->publish_state(this->warning_power_limit);}
        if (this->warning_pv_voltage_high_) {this->warning_pv_voltage_high_->publish_state(this->warning_pv_voltage_high);}
        if (this->fault_mppt_overload_) {this->fault_mppt_overload_->publish_state(this->fault_mppt_overload);}
        if (this->warning_mppt_overload_) {this->warning_mppt_overload_->publish_state(this->warning_mppt_overload);}
        if (this->warning_battery_too_low_to_charge_) {this->warning_battery_too_low_to_charge_->publish_state(this->warning_battery_too_low_to_charge);}
        if (this->fault_dc_dc_over_current_) {this->fault_dc_dc_over_current_->publish_state(this->fault_dc_dc_over_current);}
        if (this->fault_code_) {this->fault_code_->publish_state(this->fault_code);}
        if (this->warnung_low_pv_energy_) {this->warnung_low_pv_energy_->publish_state(this->warnung_low_pv_energy);}
        if (this->warning_high_ac_input_during_bus_soft_start_) {this->warning_high_ac_input_during_bus_soft_start_->publish_state(this->warning_high_ac_input_during_bus_soft_start);}
        if (this->warning_battery_equalization_) {this->warning_battery_equalization_->publish_state(this->warning_battery_equalization);}
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QT:
        this->state_ = STATE_IDLE;
        break;

    }
  }

  if (this->state_ == STATE_POLL_CHECKED) {
    bool enabled = true;
    String fc;
    char tmp[PIPSOLAR_READ_BUFFER_LENGTH];
    sprintf(tmp,"%s",this->read_buffer_);
    switch(this->used_polling_commands_[this->last_polling_command].identifier) {
      case POLLING_QPIRI:
        ESP_LOGD(TAG,"Decode QPIRI");
        sscanf(tmp,"(%f %f %f %f %f %d %d %f %f %f %f %f %d %d %d %d %d %d %d %d %d %d %f %d %d",  
          &grid_rating_voltage,
          &grid_rating_current,
          &ac_output_rating_voltage,
          &ac_output_rating_frequency,
          &ac_output_rating_current,
          &ac_output_rating_apparent_power,
          &ac_output_rating_active_power,
          &battery_rating_voltage,
          &battery_recharge_voltage,
          &battery_under_voltage,
          &battery_bulk_voltage,
          &battery_float_voltage,
          &battery_type,
          &current_max_ac_charging_current,
          &current_max_charging_current,
          &input_voltage_range,
          &output_source_priority,
          &charger_source_priority,
          &parallel_max_num,
          &machine_type,
          &topology,
          &output_mode,
          &battery_redischarge_voltage,
          &pv_ok_condition_for_parallel,
          &pv_power_balance);
        if (this->last_qpiri_) {this->last_qpiri_->publish_state(tmp);}
        this->state_ = STATE_POLL_DECODED;
        break;
      case POLLING_QPIGS:
        ESP_LOGD(TAG,"Decode QPIGS");
        sscanf(tmp,"(%f %f %f %f %d %d %d %d %f %d %d %d %d %f %f %d %1d%1d%1d%1d%1d%1d%1d%1d %d %d %d %1d%1d%1d",  
          &grid_voltage,
          &grid_frequency,
          &ac_output_voltage,
          &ac_output_frequency,
          &ac_output_apparent_power,
          &ac_output_active_power,
          &output_load_percent,
          &bus_voltage,
          &battery_voltage,
          &battery_charging_current,
          &battery_capacity_percent,
          &inverter_heat_sink_temperature,
          &pv_input_current_for_battery,
          &pv_input_voltage,
          &battery_voltage_scc,
          &battery_discharge_current,
          &add_sbu_priority_version,
          &configuration_status,
          &scc_firmware_version,
          &load_status,
          &battery_voltage_to_steady_while_charging,
          &charging_status,
          &scc_charging_status,
          &ac_charging_status,
          &battery_voltage_offset_for_fans_on,
          &eeprom_version,
          &pv_charging_power,
          &charging_to_floating_mode,
          &switch_on,
          &dustproof_installed
        );

        if (this->last_qpigs_) {this->last_qpigs_->publish_state(tmp);}
        this->state_ = STATE_POLL_DECODED;
        break;
      case POLLING_QMOD:
        ESP_LOGD(TAG,"Decode QMOD");
        this->device_mode = char(this->read_buffer_[1]);
        if (this->last_qmod_) {this->last_qmod_->publish_state(tmp);}
        this->state_ = STATE_POLL_DECODED;
        break;
      case POLLING_QFLAG:
        ESP_LOGD(TAG,"Decode QFLAG");
        //result like:"(EbkuvxzDajy"
        //get through all char: ignore first "(" Enable flag on 'E', Disable on 'D') else set the corresponding value
        for (int i = 1; i < strlen(tmp); i++) {
          switch (tmp[i]) {
            case 'E': enabled = true; break;
            case 'D': enabled = false; break;
            case 'a': this->silence_buzzer_open_buzzer = enabled; break;
            case 'b': this->overload_bypass_function = enabled; break;
            case 'k': this->lcd_escape_to_default = enabled; break;
            case 'u': this->overload_restart_function = enabled; break;
            case 'v': this->over_temperature_restart_function = enabled; break;
            case 'x': this->backlight_on = enabled; break;
            case 'y': this->alarm_on_when_primary_source_interrupt = enabled; break;
            case 'z': this->fault_code_record = enabled; break;
            case 'j': this->power_saving = enabled; break;

          }
        }
        if (this->last_qflag_) {this->last_qflag_->publish_state(tmp);}
        this->state_ = STATE_POLL_DECODED;
        break;
      case POLLING_QPIWS:
        ESP_LOGD(TAG,"Decode QPIWS");
        // '(00000000000000000000000000000000'
        //iterate over all available flag (as not all models have all flags, but at least in the same order)
        this->warnings_present = 0;
        this->faults_present = 0;

        for (int i = 1; i < strlen(tmp); i++) {
          enabled = tmp[i] == '1';
          switch (i) {
            case 1:   this->warning_power_loss = enabled; this->warnings_present += enabled; break;
            case 2:   this->fault_inverter_fault = enabled; this->faults_present += enabled; break;
            case 3:   this->fault_bus_over = enabled; this->faults_present += enabled; break;
            case 4:   this->fault_bus_under = enabled; this->faults_present += enabled; break;
            case 5:   this->fault_bus_soft_fail = enabled; this->faults_present += enabled; break;
            case 6:   this->warning_line_fail = enabled; this->warnings_present += enabled; break;
            case 7:   this->fault_opvshort = enabled; this->faults_present += enabled; break;
            case 8:   this->fault_inverter_voltage_too_low = enabled; this->faults_present += enabled; break;
            case 9:   this->fault_inverter_voltage_too_high = enabled; this->faults_present += enabled; break;
            case 10:  this->warning_over_temperature = enabled; this->warnings_present += enabled; break;
            case 11:  this->warning_fan_lock = enabled; this->warnings_present += enabled; break;
            case 12:  this->warning_battery_voltage_high = enabled; this->warnings_present += enabled; break;
            case 13:  this->warning_battery_low_alarm = enabled; this->warnings_present += enabled; break;
            case 15:  this->warning_battery_under_shutdown = enabled; this->warnings_present += enabled; break;
            case 16:  this->warning_battery_derating = enabled; this->warnings_present += enabled; break;
            case 17:  this->warning_over_load = enabled; this->warnings_present += enabled; break;
            case 18:  this->warning_eeprom_failed = enabled; this->warnings_present += enabled; break;
            case 19:  this->fault_inverter_over_current = enabled; this->faults_present += enabled; break;
            case 20:  this->fault_inverter_soft_failed = enabled; this->faults_present += enabled; break;
            case 21:  this->fault_self_test_failed = enabled; this->faults_present += enabled; break;
            case 22:  this->fault_op_dc_voltage_over = enabled; this->faults_present += enabled; break;
            case 23:  this->fault_battery_open = enabled; this->faults_present += enabled; break;
            case 24:  this->fault_current_sensor_failed = enabled; this->faults_present += enabled; break;
            case 25:  this->fault_battery_short = enabled; this->faults_present += enabled; break;
            case 26:  this->warning_power_limit = enabled; this->warnings_present += enabled; break;
            case 27:  this->warning_pv_voltage_high = enabled; this->warnings_present += enabled; break;
            case 28:  this->fault_mppt_overload = enabled; this->faults_present += enabled; break;
            case 29:  this->warning_mppt_overload = enabled; this->warnings_present += enabled; break;
            case 30:  this->warning_battery_too_low_to_charge = enabled; this->warnings_present += enabled; break;
            case 31:  this->fault_dc_dc_over_current = enabled; this->faults_present += enabled; break;
            case 32:  
                      fc = String(tmp[i]);
                      fc.concat(tmp[i+1]);
                      this->fault_code = fc.toInt();
                      break;
            case 34:  this->warnung_low_pv_energy = enabled; this->warnings_present += enabled; break;
            case 35:  this->warning_high_ac_input_during_bus_soft_start = enabled; this->warnings_present += enabled; break;
            case 36:  this->warning_battery_equalization = enabled; this->warnings_present += enabled; break;

          }
        }
        if (this->last_qpiws_) {this->last_qpiws_->publish_state(tmp);}
        this->state_ = STATE_POLL_DECODED;
        break;
      case POLLING_QT:
        ESP_LOGD(TAG,"Decode QT");
        if (this->last_qt_) {this->last_qt_->publish_state(tmp);}
        this->state_ = STATE_POLL_DECODED;
        break;

      default:
        this->state_ = STATE_IDLE;
        break;
    }
    return;
  }

  if (this->state_ == STATE_POLL_COMPLETE ) {
    if (this->check_incoming_crc()) {
      //crc ok
      this->state_ = STATE_POLL_CHECKED;
      return;
    } else {
      this->state_ = STATE_IDLE;
    }
  }

  if (this->state_ == STATE_COMMAND || this->state_ == STATE_POLL) {
    while (this->available()) {
      uint8_t byte;
      this->read_byte(&byte);

      if (this->read_pos_ == PIPSOLAR_READ_BUFFER_LENGTH)
      {
        this->read_pos_ = 0;
        this->empty_uart_buffer();
      }
      this->read_buffer_[this->read_pos_] = byte;
      this->read_pos_++;

      //end of answer
      if (byte == 0x0D) {
        this->read_buffer_[this->read_pos_] = 0;
        this->empty_uart_buffer();
        if (this->state_ == STATE_POLL) {this->state_ = STATE_POLL_COMPLETE;}
        if (this->state_ == STATE_COMMAND) {this->state_ = STATE_COMMAND_COMPLETE;}
      }
    } // available
  }
  if (this->state_ == STATE_COMMAND) {
    if (millis() - this->command_start_millis_ > this->COMMAND_TIMEOUT) {
      // command timeout
      const char* command = this->command_queue_[this->command_queue_position_].c_str();
      this->command_start_millis_ = millis();
      ESP_LOGD(TAG,"timeout command from queue: %s",command);
      this->command_queue_[this->command_queue_position_] = String("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
      return;
    } else {

    }
  }
  if (this->state_ == STATE_POLL) {
    if (millis() - this->command_start_millis_ > this->COMMAND_TIMEOUT) {
      // command timeout
      ESP_LOGD(TAG,"timeout command to poll: %s",this->used_polling_commands_[this->last_polling_command].command);
      this->state_ = STATE_IDLE;
    } else {

    }

  }


}

uint8_t Pipsolar::check_incoming_length(uint8_t length) {
  if (this->read_pos_ - 3 == length) {
    return 1;
  }
  return 0;
}

uint8_t Pipsolar::check_incoming_crc() {
  uint16_t crc16;
  crc16 = calc_crc(read_buffer_,read_pos_ - 3);
  ESP_LOGD(TAG,"checking crc on incoming message");
  if (highByte(crc16) == read_buffer_[read_pos_-3] && lowByte(crc16) == read_buffer_[read_pos_-2]) {  				
		ESP_LOGD(TAG, "CRC OK");
		read_buffer_[read_pos_-1]=0;
		read_buffer_[read_pos_-2]=0;
		read_buffer_[read_pos_-3]=0;
		return 1;

	} 
  ESP_LOGD(TAG, "CRC NOK expected: %X %X but got: %X %X",highByte(crc16),lowByte(crc16),read_buffer_[read_pos_-3],read_buffer_[read_pos_-2]);

  return 0;
}

//send next command used
uint8_t Pipsolar::send_next_command() {
   	uint16_t crc16;
    if (this->command_queue_[this->command_queue_position_].length() != 0) {
      const char* command = this->command_queue_[this->command_queue_position_].c_str();
      uint8_t byte_command[16];
      this->command_queue_[this->command_queue_position_].getBytes(byte_command,16);
      uint8_t length = this->command_queue_[this->command_queue_position_].length();
      this->state_ = STATE_COMMAND;
      this->command_start_millis_ = millis();
      this->empty_uart_buffer();
      this->read_pos_ = 0;
      crc16 = calc_crc(byte_command,length);
      this->write_str(command);
      // checksum
      this->write(highByte(crc16));
      this->write(lowByte(crc16)); 
      // end Byte
      this->write(0x0D);  
      ESP_LOGD(TAG,"Sending command from queue: %s with length %d",command, length);
      return 1;
    }
    return 0;
}

void Pipsolar::send_next_poll() {
 	uint16_t crc16;
  this->last_polling_command = (this->last_polling_command + 1) % 15;
  if (this->used_polling_commands_[this->last_polling_command].length == 0) {
    this->last_polling_command = 0;
  }
  if (this->used_polling_commands_[this->last_polling_command].length == 0) {
    // no command specified
    return;
  }
  this->state_ = STATE_POLL;
  this->command_start_millis_ = millis();
  this->empty_uart_buffer();
  this->read_pos_ = 0;
  crc16 = calc_crc(this->used_polling_commands_[this->last_polling_command].command,this->used_polling_commands_[this->last_polling_command].length);
  this->write_array(this->used_polling_commands_[this->last_polling_command].command,this->used_polling_commands_[this->last_polling_command].length);
  // checksum
  this->write(highByte(crc16));
  this->write(lowByte(crc16)); 
  // end Byte
  this->write(0x0D);  
  ESP_LOGD(TAG,"Sending polling command : %s with length %d",this->used_polling_commands_[this->last_polling_command].command, this->used_polling_commands_[this->last_polling_command].length);

}

void Pipsolar::queue_command(const char *command, byte length) {
  byte next_position = command_queue_position_;
  for (byte i = 0; i < COMMAND_QUEUE_LENGTH; i++)
  {
    byte testposition = (next_position + i) % COMMAND_QUEUE_LENGTH;
    if (command_queue_[testposition].length() == 0) {
      command_queue_[testposition] = String(command);
      ESP_LOGD(TAG,"Command queued successfully: %s with length %d at position %d",command,command_queue_[testposition].length(),testposition);
      return;
    }
  }

  ESP_LOGD(TAG,"Command queue full dropping command: %s",command);
}

void Pipsolar::switch_command(String command) {
  ESP_LOGD(TAG,"got command: %s",command.c_str());

  queue_command(command.c_str(),command.length());
  // if (command.equals(String("ospu"))) {
  //   // set output source priority to utility
  //   queue_command("POP00",5);
  //   this->output_source_priority_utility_switch_->publish_state(true);
  //   this->output_source_priority_solar_switch_->publish_state(false);
  //   this->output_source_priority_battery_switch_->publish_state(false);
  // }
  // if (command.equals(String("osps"))) {
  //   // set output source priority to solar
  //   queue_command("POP01",5);
  //   this->output_source_priority_utility_switch_->publish_state(false);
  //   this->output_source_priority_solar_switch_->publish_state(true);
  //   this->output_source_priority_battery_switch_->publish_state(false);
  // }
  // if (command.equals(String("ospb"))) {
  //   // set output source priority to battery
  //   queue_command("POP02",5);
  //   this->output_source_priority_utility_switch_->publish_state(false);
  //   this->output_source_priority_solar_switch_->publish_state(false);
  //   this->output_source_priority_battery_switch_->publish_state(true);
  // }
}
void Pipsolar::dump_config() {
  ESP_LOGCONFIG(TAG, "Pipsolar:");
  ESP_LOGCONFIG(TAG, "used commands:");
  for (uint8_t i = 0; i < 15; i++) {
    if (this->used_polling_commands_[i].length != 0) {
      ESP_LOGCONFIG(TAG, "%s",this->used_polling_commands_[i].command);
    }
  }
  

}
void Pipsolar::update() {
  if (!this->update_running)
  {
    this->update_running = 1;
  }
}

void Pipsolar::add_polling_command(const char* command, PollingCommand polling_command) {
  for (uint8_t c = 0; c < 15; c++) {
    if (this->used_polling_commands_[c].length == strlen(command)) {
      uint8_t len = strlen(command);
      if (memcmp(this->used_polling_commands_[c].command,command,len) == 0) {
        return;
      }
    }

    if (this->used_polling_commands_[c].length == 0) {
      size_t length = strlen(command) + 1;
      const char* beg = command;
      const char* end = command + length;
      this->used_polling_commands_[c].command = new uint8_t[length];
      size_t i = 0;
      for (; beg != end; ++beg, ++i)
      {
        this->used_polling_commands_[c].command[i] = (uint8_t)(*beg);
      }
      this->used_polling_commands_[c].errors = 0;
      this->used_polling_commands_[c].identifier = polling_command;
      this->used_polling_commands_[c].length = length-1;
      return;
    }
  }
}

uint16_t Pipsolar::calc_crc(uint8_t *msg,int n)
{
  // Initial value. xmodem uses 0xFFFF but this example
  // requires an initial value of zero.
  uint16_t x = 0;

  while(n--) {
    x = crc_xmodem_update(x, (uint16_t)*msg++);
  }
  return(x);
}

// See bottom of this page: http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
// Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
uint16_t Pipsolar::crc_xmodem_update (uint16_t crc, uint8_t data)
{
  int i;

  crc = crc ^ ((uint16_t)data << 8);
  for (i=0; i<8; i++) {
    if (crc & 0x8000)
      crc = (crc << 1) ^ 0x1021; //(polynomial = 0x1021)
    else
      crc <<= 1;
  }
  return crc;
}

}  // namespace pipsolar
}  // namespace esphome
