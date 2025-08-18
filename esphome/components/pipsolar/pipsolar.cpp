#include "pipsolar.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pipsolar {

static const char *const TAG = "pipsolar";

void Pipsolar::setup() {
  this->state_ = STATE_IDLE;
  this->command_start_millis_ = 0;
}

void Pipsolar::empty_uart_buffer_() {
  uint8_t byte;
  while (this->available()) {
    this->read_byte(&byte);
  }
}

void Pipsolar::loop() {
  // Read message
  if (this->state_ == STATE_IDLE) {
    this->empty_uart_buffer_();

    if (this->send_next_command_()) {
      // command sent
      return;
    }

    if (this->send_next_poll_()) {
      // poll sent
      return;
    }

    return;
  }
  if (this->state_ == STATE_COMMAND_COMPLETE) {
    if (this->check_incoming_length_(4)) {
      ESP_LOGD(TAG, "response length for command OK");
      if (this->check_incoming_crc_()) {
        // crc ok
        if (this->read_buffer_[1] == 'A' && this->read_buffer_[2] == 'C' && this->read_buffer_[3] == 'K') {
          ESP_LOGD(TAG, "command successful");
        } else {
          ESP_LOGD(TAG, "command not successful");
        }
        this->command_queue_[this->command_queue_position_] = std::string("");
        this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
        this->state_ = STATE_IDLE;

      } else {
        // crc failed
        this->command_queue_[this->command_queue_position_] = std::string("");
        this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
        this->state_ = STATE_IDLE;
      }
    } else {
      ESP_LOGD(TAG, "response length for command %s not OK: with length %zu",
               this->command_queue_[this->command_queue_position_].c_str(), this->read_pos_);
      this->command_queue_[this->command_queue_position_] = std::string("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
    }
  }

  if (this->state_ == STATE_POLL_CHECKED) {
    switch (this->enabled_polling_commands_[this->last_polling_command_].identifier) {
      case POLLING_QPIRI:
        ESP_LOGD(TAG, "Decode QPIRI");
        handle_qpiri_((const char*)this->read_buffer_);
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QPIGS:
        ESP_LOGD(TAG, "Decode QPIGS");
        handle_qpigs_((const char*)this->read_buffer_);
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QMOD:
        ESP_LOGD(TAG, "Decode QMOD");
        handle_qmod_((const char*)this->read_buffer_);
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QFLAG:
        ESP_LOGD(TAG, "Decode QFLAG");
        handle_qflag_((const char*)this->read_buffer_);
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QPIWS:
        ESP_LOGD(TAG, "Decode QPIWS");
        handle_qpiws_((const char*)this->read_buffer_);
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QT:
        ESP_LOGD(TAG, "Decode QT");
        handle_qt_((const char*)this->read_buffer_);
        this->state_ = STATE_IDLE;
        break;
      case POLLING_QMN:
        ESP_LOGD(TAG, "Decode QMN");
        handle_qmn_((const char*)this->read_buffer_);
        this->state_ = STATE_IDLE;
        break;
      default:
        this->state_ = STATE_IDLE;
        break;
    }
    return;
  }

  if (this->state_ == STATE_POLL_COMPLETE) {
    if (this->check_incoming_crc_()) {
      if (this->read_buffer_[0] == '(' && this->read_buffer_[1] == 'N' && this->read_buffer_[2] == 'A' &&
          this->read_buffer_[3] == 'K') {
        this->state_ = STATE_IDLE;
        return;
      }
      // crc ok
      this->enabled_polling_commands_[this->last_polling_command_].needs_update = false;
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

      if (this->read_pos_ == PIPSOLAR_READ_BUFFER_LENGTH) {
        this->read_pos_ = 0;
        this->empty_uart_buffer_();
      }
      this->read_buffer_[this->read_pos_] = byte;
      this->read_pos_++;

      // end of answer
      if (byte == 0x0D) {
        this->read_buffer_[this->read_pos_] = 0;
        this->empty_uart_buffer_();
        if (this->state_ == STATE_POLL) {
          this->state_ = STATE_POLL_COMPLETE;
        }
        if (this->state_ == STATE_COMMAND) {
          this->state_ = STATE_COMMAND_COMPLETE;
        }
      }
    }  // available
  }
  if (this->state_ == STATE_COMMAND) {
    if (millis() - this->command_start_millis_ > esphome::pipsolar::Pipsolar::COMMAND_TIMEOUT) {
      // command timeout
      const char *command = this->command_queue_[this->command_queue_position_].c_str();
      this->command_start_millis_ = millis();
      ESP_LOGD(TAG, "timeout command from queue: %s", command);
      this->command_queue_[this->command_queue_position_] = std::string("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
      return;
    } else {
    }
  }
  if (this->state_ == STATE_POLL) {
    if (millis() - this->command_start_millis_ > esphome::pipsolar::Pipsolar::COMMAND_TIMEOUT) {
      // command timeout
      ESP_LOGD(TAG, "timeout command to poll: %s", this->enabled_polling_commands_[this->last_polling_command_].command);
      this->state_ = STATE_IDLE;
    } else {
    }
  }
}

uint8_t Pipsolar::check_incoming_length_(uint8_t length) {
  if (this->read_pos_ - 3 == length) {
    return 1;
  }
  return 0;
}

uint8_t Pipsolar::check_incoming_crc_() {
  uint16_t crc16;
  crc16 = this->pipsolar_crc_(read_buffer_, read_pos_ - 3);
  ESP_LOGD(TAG, "checking crc on incoming message");
  if (((uint8_t) ((crc16) >> 8)) == read_buffer_[read_pos_ - 3] &&
      ((uint8_t) ((crc16) &0xff)) == read_buffer_[read_pos_ - 2]) {
    ESP_LOGD(TAG, "CRC OK");
    read_buffer_[read_pos_ - 1] = 0;
    read_buffer_[read_pos_ - 2] = 0;
    read_buffer_[read_pos_ - 3] = 0;
    return 1;
  }
  ESP_LOGD(TAG, "CRC NOK expected: %X %X but got: %X %X", ((uint8_t) ((crc16) >> 8)), ((uint8_t) ((crc16) &0xff)),
           read_buffer_[read_pos_ - 3], read_buffer_[read_pos_ - 2]);
  return 0;
}

// send next command from queue
bool Pipsolar::send_next_command_() {
  uint16_t crc16;
  if (!this->command_queue_[this->command_queue_position_].empty()) {
    const char *command = this->command_queue_[this->command_queue_position_].c_str();
    uint8_t byte_command[16];
    uint8_t length = this->command_queue_[this->command_queue_position_].length();
    for (uint8_t i = 0; i < length; i++) {
      byte_command[i] = (uint8_t) this->command_queue_[this->command_queue_position_].at(i);
    }
    this->state_ = STATE_COMMAND;
    this->command_start_millis_ = millis();
    this->empty_uart_buffer_();
    this->read_pos_ = 0;
    crc16 = this->pipsolar_crc_(byte_command, length);
    this->write_str(command);
    // checksum
    this->write(((uint8_t) ((crc16) >> 8)));   // highbyte
    this->write(((uint8_t) ((crc16) &0xff)));  // lowbyte
    // end Byte
    this->write(0x0D);
    ESP_LOGD(TAG, "Sending command from queue: %s with length %d", command, length);
    return true;
  }
  return false;
}

bool Pipsolar::send_next_poll_() {
  uint16_t crc16;

  for (uint8_t i = 0; i < POLLING_COMMANDS_MAX; i++) {
    this->last_polling_command_ = (this->last_polling_command_ + 1) % POLLING_COMMANDS_MAX;
    if (this->enabled_polling_commands_[this->last_polling_command_].length == 0) {
      // not enabled
      continue;
    }
    if(!this->enabled_polling_commands_[this->last_polling_command_].needs_update) {
      // no update requested
      continue;
    }
    this->state_ = STATE_POLL;
    this->command_start_millis_ = millis();
    this->empty_uart_buffer_();
    this->read_pos_ = 0;
    crc16 = this->pipsolar_crc_(this->enabled_polling_commands_[this->last_polling_command_].command,
                                this->enabled_polling_commands_[this->last_polling_command_].length);
    this->write_array(this->enabled_polling_commands_[this->last_polling_command_].command,
                      this->enabled_polling_commands_[this->last_polling_command_].length);
    // checksum
    this->write(((uint8_t) ((crc16) >> 8)));   // highbyte
    this->write(((uint8_t) ((crc16) &0xff)));  // lowbyte
    // end Byte
    this->write(0x0D);
    ESP_LOGD(TAG, "Sending polling command : %s with length %d",
            this->enabled_polling_commands_[this->last_polling_command_].command,
            this->enabled_polling_commands_[this->last_polling_command_].length);
    return true;
  }
  return false;
}

void Pipsolar::queue_command(const std::string &command) {
  uint8_t next_position = command_queue_position_;
  for (uint8_t i = 0; i < COMMAND_QUEUE_LENGTH; i++) {
    uint8_t testposition = (next_position + i) % COMMAND_QUEUE_LENGTH;
    if (command_queue_[testposition].empty()) {
      command_queue_[testposition] = command;
      ESP_LOGD(TAG, "Command queued successfully: %s with length %u at position %d", command,
               command_queue_[testposition].length(), testposition);
      return;
    }
  }
  ESP_LOGD(TAG, "Command queue full dropping command: %s", command);
}

void Pipsolar::handle_qpiri_(const char* message) {
  QPIRIValues values = QPIRIValues();

  sscanf(message, "(%f %f %f %f %f %d %d %f %f %f %f %f %d %d %d %d %d %d %d %d %d %d %f %d %d",       // NOLINT
          &values.grid_rating_voltage, &values.grid_rating_current, &values.ac_output_rating_voltage,  // NOLINT
          &values.ac_output_rating_frequency, &values.ac_output_rating_current,                        // NOLINT
          &values.ac_output_rating_apparent_power, &values.ac_output_rating_active_power,              // NOLINT
          &values.battery_rating_voltage, &values.battery_recharge_voltage,                            // NOLINT
          &values.battery_under_voltage, &values.battery_bulk_voltage, &values.battery_float_voltage,  // NOLINT
          &values.battery_type, &values.current_max_ac_charging_current,                               // NOLINT
          &values.current_max_charging_current, &values.input_voltage_range,                           // NOLINT
          &values.output_source_priority, &values.charger_source_priority, &values.parallel_max_num,   // NOLINT
          &values.machine_type, &values.topology, &values.output_mode,                                 // NOLINT
          &values.battery_redischarge_voltage, &values.pv_ok_condition_for_parallel,                   // NOLINT
          &values.pv_power_balance);                                                                   // NOLINT
  if (this->last_qpiri_) {
    this->last_qpiri_->publish_state(message);
  }

  if (this->grid_rating_voltage_) {
    this->grid_rating_voltage_->publish_state(values.grid_rating_voltage);
  }
  if (this->grid_rating_current_) {
    this->grid_rating_current_->publish_state(values.grid_rating_current);
  }
  if (this->ac_output_rating_voltage_) {
    this->ac_output_rating_voltage_->publish_state(values.ac_output_rating_voltage);
  }
  if (this->ac_output_rating_frequency_) {
    this->ac_output_rating_frequency_->publish_state(values.ac_output_rating_frequency);
  }
  if (this->ac_output_rating_current_) {
    this->ac_output_rating_current_->publish_state(values.ac_output_rating_current);
  }
  if (this->ac_output_rating_apparent_power_) {
    this->ac_output_rating_apparent_power_->publish_state(values.ac_output_rating_apparent_power);
  }
  if (this->ac_output_rating_active_power_) {
    this->ac_output_rating_active_power_->publish_state(values.ac_output_rating_active_power);
  }
  if (this->battery_rating_voltage_) {
    this->battery_rating_voltage_->publish_state(values.battery_rating_voltage);
  }
  if (this->battery_recharge_voltage_) {
    this->battery_recharge_voltage_->publish_state(values.battery_recharge_voltage);
  }
  if (this->battery_under_voltage_) {
    this->battery_under_voltage_->publish_state(values.battery_under_voltage);
  }
  if (this->battery_bulk_voltage_) {
    this->battery_bulk_voltage_->publish_state(values.battery_bulk_voltage);
  }
  if (this->battery_float_voltage_) {
    this->battery_float_voltage_->publish_state(values.battery_float_voltage);
  }
  if (this->battery_type_) {
    this->battery_type_->publish_state(values.battery_type);
  }
  if (this->current_max_ac_charging_current_) {
    this->current_max_ac_charging_current_->publish_state(values.current_max_ac_charging_current);
  }
  if (this->current_max_charging_current_) {
    this->current_max_charging_current_->publish_state(values.current_max_charging_current);
  }
  if (this->input_voltage_range_) {
    this->input_voltage_range_->publish_state(values.input_voltage_range);
  }
  // special for input voltage range switch
  if (this->input_voltage_range_switch_) {
    this->input_voltage_range_switch_->publish_state(values.input_voltage_range == 1);
  }
  if (this->output_source_priority_) {
    this->output_source_priority_->publish_state(values.output_source_priority);
  }
  // special for output source priority switches
  if (this->output_source_priority_utility_switch_) {
    this->output_source_priority_utility_switch_->publish_state(values.output_source_priority == 0);
  }
  if (this->output_source_priority_solar_switch_) {
    this->output_source_priority_solar_switch_->publish_state(values.output_source_priority == 1);
  }
  if (this->output_source_priority_battery_switch_) {
    this->output_source_priority_battery_switch_->publish_state(values.output_source_priority == 2);
  }
  if (this->output_source_priority_hybrid_switch_) {
    this->output_source_priority_hybrid_switch_->publish_state(values.output_source_priority == 3);
  }
  if (this->charger_source_priority_) {
    this->charger_source_priority_->publish_state(values.charger_source_priority);
  }
  if (this->parallel_max_num_) {
    this->parallel_max_num_->publish_state(values.parallel_max_num);
  }
  if (this->machine_type_) {
    this->machine_type_->publish_state(values.machine_type);
  }
  if (this->topology_) {
    this->topology_->publish_state(values.topology);
  }
  if (this->output_mode_) {
    this->output_mode_->publish_state(values.output_mode);
  }
  if (this->battery_redischarge_voltage_) {
    this->battery_redischarge_voltage_->publish_state(values.battery_redischarge_voltage);
  }
  if (this->pv_ok_condition_for_parallel_) {
    this->pv_ok_condition_for_parallel_->publish_state(values.pv_ok_condition_for_parallel);
  }
  // special for pv ok condition switch
  if (this->pv_ok_condition_for_parallel_switch_) {
    this->pv_ok_condition_for_parallel_switch_->publish_state(values.pv_ok_condition_for_parallel == 1);
  }
  if (this->pv_power_balance_) {
    this->pv_power_balance_->publish_state(values.pv_power_balance == 1);
  }
  // special for power balance switch
  if (this->pv_power_balance_switch_) {
    this->pv_power_balance_switch_->publish_state(values.pv_power_balance == 1);
  }
}

void Pipsolar::handle_qpigs_(const char* message) {
  QPIGSValues values = QPIGSValues();

  sscanf(                                                                                              // NOLINT
      message,                                                                                         // NOLINT
      "(%f %f %f %f %d %d %d %d %f %d %d %d %f %f %f %d %1d%1d%1d%1d%1d%1d%1d%1d %d %d %d %1d%1d%1d",  // NOLINT
      &values.grid_voltage, &values.grid_frequency, &values.ac_output_voltage,                         // NOLINT
      &values.ac_output_frequency,                                                                     // NOLINT
      &values.ac_output_apparent_power, &values.ac_output_active_power, &values.output_load_percent,   // NOLINT
      &values.bus_voltage, &values.battery_voltage, &values.battery_charging_current,                  // NOLINT
      &values.battery_capacity_percent, &values.inverter_heat_sink_temperature,                        // NOLINT
      &values.pv_input_current_for_battery, &values.pv_input_voltage, &values.battery_voltage_scc,     // NOLINT
      &values.battery_discharge_current, &values.add_sbu_priority_version,                             // NOLINT
      &values.configuration_status, &values.scc_firmware_version, &values.load_status,                 // NOLINT
      &values.battery_voltage_to_steady_while_charging, &values.charging_status,                       // NOLINT
      &values.scc_charging_status, &values.ac_charging_status,                                         // NOLINT
      &values.battery_voltage_offset_for_fans_on, &values.eeprom_version, &values.pv_charging_power,   // NOLINT
      &values.charging_to_floating_mode, &values.switch_on,                                            // NOLINT
      &values.dustproof_installed);                                                                    // NOLINT
  if (this->last_qpigs_) {
    this->last_qpigs_->publish_state(message);
  }

  if (this->grid_voltage_) {
    this->grid_voltage_->publish_state(values.grid_voltage);
  }
  if (this->grid_frequency_) {
    this->grid_frequency_->publish_state(values.grid_frequency);
  }
  if (this->ac_output_voltage_) {
    this->ac_output_voltage_->publish_state(values.ac_output_voltage);
  }
  if (this->ac_output_frequency_) {
    this->ac_output_frequency_->publish_state(values.ac_output_frequency);
  }
  if (this->ac_output_apparent_power_) {
    this->ac_output_apparent_power_->publish_state(values.ac_output_apparent_power);
  }
  if (this->ac_output_active_power_) {
    this->ac_output_active_power_->publish_state(values.ac_output_active_power);
  }
  if (this->output_load_percent_) {
    this->output_load_percent_->publish_state(values.output_load_percent);
  }
  if (this->bus_voltage_) {
    this->bus_voltage_->publish_state(values.bus_voltage);
  }
  if (this->battery_voltage_) {
    this->battery_voltage_->publish_state(values.battery_voltage);
  }
  if (this->battery_charging_current_) {
    this->battery_charging_current_->publish_state(values.battery_charging_current);
  }
  if (this->battery_capacity_percent_) {
    this->battery_capacity_percent_->publish_state(values.battery_capacity_percent);
  }
  if (this->inverter_heat_sink_temperature_) {
    this->inverter_heat_sink_temperature_->publish_state(values.inverter_heat_sink_temperature);
  }
  if (this->pv_input_current_for_battery_) {
    this->pv_input_current_for_battery_->publish_state(values.pv_input_current_for_battery);
  }
  if (this->pv_input_voltage_) {
    this->pv_input_voltage_->publish_state(values.pv_input_voltage);
  }
  if (this->battery_voltage_scc_) {
    this->battery_voltage_scc_->publish_state(values.battery_voltage_scc);
  }
  if (this->battery_discharge_current_) {
    this->battery_discharge_current_->publish_state(values.battery_discharge_current);
  }
  if (this->add_sbu_priority_version_) {
    this->add_sbu_priority_version_->publish_state(values.add_sbu_priority_version);
  }
  if (this->configuration_status_) {
    this->configuration_status_->publish_state(values.configuration_status);
  }
  if (this->scc_firmware_version_) {
    this->scc_firmware_version_->publish_state(values.scc_firmware_version);
  }
  if (this->load_status_) {
    this->load_status_->publish_state(values.load_status);
  }
  if (this->battery_voltage_to_steady_while_charging_) {
    this->battery_voltage_to_steady_while_charging_->publish_state(
        values.battery_voltage_to_steady_while_charging);
  }
  if (this->charging_status_) {
    this->charging_status_->publish_state(values.charging_status);
  }
  if (this->scc_charging_status_) {
    this->scc_charging_status_->publish_state(values.scc_charging_status);
  }
  if (this->ac_charging_status_) {
    this->ac_charging_status_->publish_state(values.ac_charging_status);
  }
  if (this->battery_voltage_offset_for_fans_on_) {
    this->battery_voltage_offset_for_fans_on_->publish_state(values.battery_voltage_offset_for_fans_on / 10.0f);
  }  //.1 scale
  if (this->eeprom_version_) {
    this->eeprom_version_->publish_state(values.eeprom_version);
  }
  if (this->pv_charging_power_) {
    this->pv_charging_power_->publish_state(values.pv_charging_power);
  }
  if (this->charging_to_floating_mode_) {
    this->charging_to_floating_mode_->publish_state(values.charging_to_floating_mode);
  }
  if (this->switch_on_) {
    this->switch_on_->publish_state(values.switch_on);
  }
  if (this->dustproof_installed_) {
    this->dustproof_installed_->publish_state(values.dustproof_installed);
  }
}

void Pipsolar::handle_qmod_(const char* message) {
  std::string mode;
  char device_mode = char(message[1]);
  if (this->last_qmod_) {
    this->last_qmod_->publish_state(message);
  }
  if (this->device_mode_) {
    mode = device_mode;
    this->device_mode_->publish_state(mode);
  }
}

void Pipsolar::handle_qflag_(const char* message) {
  // result like:"(EbkuvxzDajy"
  // get through all char: ignore first "(" Enable flag on 'E', Disable on 'D') else set the corresponding value
  QFLAGValues values = QFLAGValues();
  bool enabled = true;
  for (size_t i = 1; i < strlen(message); i++) {
    switch (message[i]) {
      case 'E':
        enabled = true;
        break;
      case 'D':
        enabled = false;
        break;
      case 'a':
        values.silence_buzzer_open_buzzer = enabled;
        break;
      case 'b':
        values.overload_bypass_function = enabled;
        break;
      case 'k':
        values.lcd_escape_to_default = enabled;
        break;
      case 'u':
        values.overload_restart_function = enabled;
        break;
      case 'v':
        values.over_temperature_restart_function = enabled;
        break;
      case 'x':
        values.backlight_on = enabled;
        break;
      case 'y':
        values.alarm_on_when_primary_source_interrupt = enabled;
        break;
      case 'z':
        values.fault_code_record = enabled;
        break;
      case 'j':
        values.power_saving = enabled;
        break;
    }
  }
  if (this->last_qflag_) {
    this->last_qflag_->publish_state(message);
  }

  if (this->silence_buzzer_open_buzzer_) {
    this->silence_buzzer_open_buzzer_->publish_state(values.silence_buzzer_open_buzzer);
  }
  if (this->overload_bypass_function_) {
    this->overload_bypass_function_->publish_state(values.overload_bypass_function);
  }
  if (this->lcd_escape_to_default_) {
    this->lcd_escape_to_default_->publish_state(values.lcd_escape_to_default);
  }
  if (this->overload_restart_function_) {
    this->overload_restart_function_->publish_state(values.overload_restart_function);
  }
  if (this->over_temperature_restart_function_) {
    this->over_temperature_restart_function_->publish_state(values.over_temperature_restart_function);
  }
  if (this->backlight_on_) {
    this->backlight_on_->publish_state(values.backlight_on);
  }
  if (this->alarm_on_when_primary_source_interrupt_) {
    this->alarm_on_when_primary_source_interrupt_->publish_state(values.alarm_on_when_primary_source_interrupt);
  }
  if (this->fault_code_record_) {
    this->fault_code_record_->publish_state(values.fault_code_record);
  }
  if (this->power_saving_) {
    this->power_saving_->publish_state(values.power_saving);
  }
}

void Pipsolar::handle_qpiws_(const char* message) {
  // '(00000000000000000000000000000000'
  // iterate over all available flag (as not all models have all flags, but at least in the same order)
  QPIWSValues values = QPIWSValues();
  bool enabled = true;
  std::string fc;
  bool value_warnings_present = false;
  bool value_faults_present = true;

  for (size_t i = 1; i < strlen(message); i++) {
    enabled = message[i] == '1';
    switch (i) {
      case 1:
        values.warning_power_loss = enabled;
        value_warnings_present |= enabled;
        break;
      case 2:
        values.fault_inverter_fault = enabled;
        value_faults_present |= enabled;
        break;
      case 3:
        values.fault_bus_over = enabled;
        value_faults_present |= enabled;
        break;
      case 4:
        values.fault_bus_under = enabled;
        value_faults_present |= enabled;
        break;
      case 5:
        values.fault_bus_soft_fail = enabled;
        value_faults_present |= enabled;
        break;
      case 6:
        values.warning_line_fail = enabled;
        value_warnings_present |= enabled;
        break;
      case 7:
        values.fault_opvshort = enabled;
        value_faults_present |= enabled;
        break;
      case 8:
        values.fault_inverter_voltage_too_low = enabled;
        value_faults_present |= enabled;
        break;
      case 9:
        values.fault_inverter_voltage_too_high = enabled;
        value_faults_present |= enabled;
        break;
      case 10:
        values.warning_over_temperature = enabled;
        value_warnings_present |= enabled;
        break;
      case 11:
        values.warning_fan_lock = enabled;
        value_warnings_present |= enabled;
        break;
      case 12:
        values.warning_battery_voltage_high = enabled;
        value_warnings_present |= enabled;
        break;
      case 13:
        values.warning_battery_low_alarm = enabled;
        value_warnings_present |= enabled;
        break;
      case 15:
        values.warning_battery_under_shutdown = enabled;
        value_warnings_present |= enabled;
        break;
      case 16:
        values.warning_battery_derating = enabled;
        value_warnings_present |= enabled;
        break;
      case 17:
        values.warning_over_load = enabled;
        value_warnings_present |= enabled;
        break;
      case 18:
        values.warning_eeprom_failed = enabled;
        value_warnings_present |= enabled;
        break;
      case 19:
        values.fault_inverter_over_current = enabled;
        value_faults_present |= enabled;
        break;
      case 20:
        values.fault_inverter_soft_failed = enabled;
        value_faults_present |= enabled;
        break;
      case 21:
        values.fault_self_test_failed = enabled;
        value_faults_present |= enabled;
        break;
      case 22:
        values.fault_op_dc_voltage_over = enabled;
        value_faults_present |= enabled;
        break;
      case 23:
        values.fault_battery_open = enabled;
        value_faults_present |= enabled;
        break;
      case 24:
        values.fault_current_sensor_failed = enabled;
        value_faults_present |= enabled;
        break;
      case 25:
        values.fault_battery_short = enabled;
        value_faults_present |= enabled;
        break;
      case 26:
        values.warning_power_limit = enabled;
        value_warnings_present |= enabled;
        break;
      case 27:
        values.warning_pv_voltage_high = enabled;
        value_warnings_present |= enabled;
        break;
      case 28:
        values.fault_mppt_overload = enabled;
        value_faults_present |= enabled;
        break;
      case 29:
        values.warning_mppt_overload = enabled;
        value_warnings_present |= enabled;
        break;
      case 30:
        values.warning_battery_too_low_to_charge = enabled;
        value_warnings_present |= enabled;
        break;
      case 31:
        values.fault_dc_dc_over_current = enabled;
        value_faults_present |= enabled;
        break;
      case 32:
        fc = message[i];
        fc += message[i + 1];
        values.fault_code = parse_number<int>(fc).value_or(0);
        break;
      case 34:
        values.warnung_low_pv_energy = enabled;
        value_warnings_present |= enabled;
        break;
      case 35:
        values.warning_high_ac_input_during_bus_soft_start = enabled;
        value_warnings_present |= enabled;
        break;
      case 36:
        values.warning_battery_equalization = enabled;
        value_warnings_present |= enabled;
        break;
    }
  }
  if (this->last_qpiws_) {
    this->last_qpiws_->publish_state(message);
  }

  if (this->warnings_present_) {
    this->warnings_present_->publish_state(value_warnings_present);
  }
  if (this->faults_present_) {
    this->faults_present_->publish_state(value_faults_present);
  }
  if (this->warning_power_loss_) {
    this->warning_power_loss_->publish_state(values.warning_power_loss);
  }
  if (this->fault_inverter_fault_) {
    this->fault_inverter_fault_->publish_state(values.fault_inverter_fault);
  }
  if (this->fault_bus_over_) {
    this->fault_bus_over_->publish_state(values.fault_bus_over);
  }
  if (this->fault_bus_under_) {
    this->fault_bus_under_->publish_state(values.fault_bus_under);
  }
  if (this->fault_bus_soft_fail_) {
    this->fault_bus_soft_fail_->publish_state(values.fault_bus_soft_fail);
  }
  if (this->warning_line_fail_) {
    this->warning_line_fail_->publish_state(values.warning_line_fail);
  }
  if (this->fault_opvshort_) {
    this->fault_opvshort_->publish_state(values.fault_opvshort);
  }
  if (this->fault_inverter_voltage_too_low_) {
    this->fault_inverter_voltage_too_low_->publish_state(values.fault_inverter_voltage_too_low);
  }
  if (this->fault_inverter_voltage_too_high_) {
    this->fault_inverter_voltage_too_high_->publish_state(values.fault_inverter_voltage_too_high);
  }
  if (this->warning_over_temperature_) {
    this->warning_over_temperature_->publish_state(values.warning_over_temperature);
  }
  if (this->warning_fan_lock_) {
    this->warning_fan_lock_->publish_state(values.warning_fan_lock);
  }
  if (this->warning_battery_voltage_high_) {
    this->warning_battery_voltage_high_->publish_state(values.warning_battery_voltage_high);
  }
  if (this->warning_battery_low_alarm_) {
    this->warning_battery_low_alarm_->publish_state(values.warning_battery_low_alarm);
  }
  if (this->warning_battery_under_shutdown_) {
    this->warning_battery_under_shutdown_->publish_state(values.warning_battery_under_shutdown);
  }
  if (this->warning_battery_derating_) {
    this->warning_battery_derating_->publish_state(values.warning_battery_derating);
  }
  if (this->warning_over_load_) {
    this->warning_over_load_->publish_state(values.warning_over_load);
  }
  if (this->warning_eeprom_failed_) {
    this->warning_eeprom_failed_->publish_state(values.warning_eeprom_failed);
  }
  if (this->fault_inverter_over_current_) {
    this->fault_inverter_over_current_->publish_state(values.fault_inverter_over_current);
  }
  if (this->fault_inverter_soft_failed_) {
    this->fault_inverter_soft_failed_->publish_state(values.fault_inverter_soft_failed);
  }
  if (this->fault_self_test_failed_) {
    this->fault_self_test_failed_->publish_state(values.fault_self_test_failed);
  }
  if (this->fault_op_dc_voltage_over_) {
    this->fault_op_dc_voltage_over_->publish_state(values.fault_op_dc_voltage_over);
  }
  if (this->fault_battery_open_) {
    this->fault_battery_open_->publish_state(values.fault_battery_open);
  }
  if (this->fault_current_sensor_failed_) {
    this->fault_current_sensor_failed_->publish_state(values.fault_current_sensor_failed);
  }
  if (this->fault_battery_short_) {
    this->fault_battery_short_->publish_state(values.fault_battery_short);
  }
  if (this->warning_power_limit_) {
    this->warning_power_limit_->publish_state(values.warning_power_limit);
  }
  if (this->warning_pv_voltage_high_) {
    this->warning_pv_voltage_high_->publish_state(values.warning_pv_voltage_high);
  }
  if (this->fault_mppt_overload_) {
    this->fault_mppt_overload_->publish_state(values.fault_mppt_overload);
  }
  if (this->warning_mppt_overload_) {
    this->warning_mppt_overload_->publish_state(values.warning_mppt_overload);
  }
  if (this->warning_battery_too_low_to_charge_) {
    this->warning_battery_too_low_to_charge_->publish_state(values.warning_battery_too_low_to_charge);
  }
  if (this->fault_dc_dc_over_current_) {
    this->fault_dc_dc_over_current_->publish_state(values.fault_dc_dc_over_current);
  }
  if (this->fault_code_) {
    this->fault_code_->publish_state(values.fault_code);
  }
  if (this->warnung_low_pv_energy_) {
    this->warnung_low_pv_energy_->publish_state(values.warnung_low_pv_energy);
  }
  if (this->warning_high_ac_input_during_bus_soft_start_) {
    this->warning_high_ac_input_during_bus_soft_start_->publish_state(
        values.warning_high_ac_input_during_bus_soft_start);
  }
  if (this->warning_battery_equalization_) {
    this->warning_battery_equalization_->publish_state(values.warning_battery_equalization);
  }
}

void Pipsolar::handle_qt_(const char *message) {
  if (this->last_qt_) {
    this->last_qt_->publish_state(message);
  }
}

void Pipsolar::handle_qmn_(const char *message) {
  if (this->last_qmn_) {
    this->last_qmn_->publish_state(message);
  }
}

void Pipsolar::dump_config() {
  ESP_LOGCONFIG(TAG, "Pipsolar:\n"
                     "enabled polling commands:");
  for (auto &enabled_polling_command : this->enabled_polling_commands_) {
    if (enabled_polling_command.length != 0) {
      ESP_LOGCONFIG(TAG, "%s", enabled_polling_command.command);
    }
  }
}
void Pipsolar::update() {
  for (auto &enabled_polling_command : this->enabled_polling_commands_) {
    if (enabled_polling_command.length != 0) {
      enabled_polling_command.needs_update = true;
    }
  }
}

void Pipsolar::add_polling_command_(const char *command, ENUMPollingCommand polling_command) {
  for (auto &enabled_polling_command : this->enabled_polling_commands_) {
    if (enabled_polling_command.length == strlen(command)) {
      uint8_t len = strlen(command);
      if (memcmp(enabled_polling_command.command, command, len) == 0) {
        return;
      }
    }
    if (enabled_polling_command.length == 0) {
      size_t length = strlen(command);

      enabled_polling_command.command = new uint8_t[length + 1];  // NOLINT(cppcoreguidelines-owning-memory)
      for (size_t i = 0; i < length + 1; i++) {
        enabled_polling_command.command[i] = (uint8_t) command[i];
      }
      enabled_polling_command.errors = 0;
      enabled_polling_command.identifier = polling_command;
      enabled_polling_command.length = length;
      enabled_polling_command.needs_update = true;
      return;
    }
  }
}

uint16_t Pipsolar::pipsolar_crc_(uint8_t *msg, uint8_t len) {
  uint16_t crc = crc16be(msg, len);
  uint8_t crc_low = crc & 0xff;
  uint8_t crc_high = crc >> 8;
  if (crc_low == 0x28 || crc_low == 0x0d || crc_low == 0x0a)
    crc_low++;
  if (crc_high == 0x28 || crc_high == 0x0d || crc_high == 0x0a)
    crc_high++;
  crc = (crc_high << 8) | crc_low;
  return crc;
}

}  // namespace pipsolar
}  // namespace esphome
