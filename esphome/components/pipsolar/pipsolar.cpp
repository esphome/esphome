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
        // no log message necessary, check_incoming_crc_() logs
        this->command_queue_[this->command_queue_position_] = std::string("");
        this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
        this->state_ = STATE_IDLE;
      }
    } else {
      ESP_LOGD(TAG, "command %s response length not OK: with length %zu",
               this->command_queue_[this->command_queue_position_].c_str(), this->read_pos_);
      this->command_queue_[this->command_queue_position_] = std::string("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
    }
  }

  if (this->state_ == STATE_POLL_CHECKED) {
    ESP_LOGD(TAG, "poll %s decode", this->enabled_polling_commands_[this->last_polling_command_].command);
    this->handle_poll_response_(this->enabled_polling_commands_[this->last_polling_command_].identifier,
                                (const char *) this->read_buffer_);
    this->state_ = STATE_IDLE;
    return;
  }

  if (this->state_ == STATE_POLL_COMPLETE) {
    if (this->check_incoming_crc_()) {
      if (this->read_buffer_[0] == '(' && this->read_buffer_[1] == 'N' && this->read_buffer_[2] == 'A' &&
          this->read_buffer_[3] == 'K') {
        ESP_LOGD(TAG, "poll %s NACK", this->enabled_polling_commands_[this->last_polling_command_].command);
        this->handle_poll_error_(this->enabled_polling_commands_[this->last_polling_command_].identifier);
        this->state_ = STATE_IDLE;
        return;
      }
      // crc ok
      this->enabled_polling_commands_[this->last_polling_command_].needs_update = false;
      this->state_ = STATE_POLL_CHECKED;
      return;
    } else {
      // crc failed
      // no log message necessary, check_incoming_crc_() logs
      this->handle_poll_error_(this->enabled_polling_commands_[this->last_polling_command_].identifier);
      this->state_ = STATE_IDLE;
    }
  }

  if (this->state_ == STATE_COMMAND || this->state_ == STATE_POLL) {
    while (this->available()) {
      uint8_t byte;
      this->read_byte(&byte);

      // make sure data and null terminator fit in buffer
      if (this->read_pos_ >= PIPSOLAR_READ_BUFFER_LENGTH - 1) {
        this->read_pos_ = 0;
        this->empty_uart_buffer_();
        ESP_LOGW(TAG, "response data too long, discarding.");
        break;
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
      ESP_LOGD(TAG, "command %s timeout", command);
      this->command_queue_[this->command_queue_position_] = std::string("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
      return;
    }
  }
  if (this->state_ == STATE_POLL) {
    if (millis() - this->command_start_millis_ > esphome::pipsolar::Pipsolar::COMMAND_TIMEOUT) {
      // command timeout
      ESP_LOGD(TAG, "poll %s timeout", this->enabled_polling_commands_[this->last_polling_command_].command);
      this->handle_poll_error_(this->enabled_polling_commands_[this->last_polling_command_].identifier);
      this->state_ = STATE_IDLE;
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
    if (!this->enabled_polling_commands_[this->last_polling_command_].needs_update) {
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
    ESP_LOGD(TAG, "Sending polling command: %s with length %d",
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
      ESP_LOGD(TAG, "Command queued successfully: %s at position %d", command.c_str(), testposition);
      return;
    }
  }
  ESP_LOGD(TAG, "Command queue full dropping command: %s", command.c_str());
}

void Pipsolar::handle_poll_response_(ENUMPollingCommand polling_command, const char *message) {
  switch (polling_command) {
    case POLLING_QPIRI:
      handle_qpiri_(message);
      break;
    case POLLING_QPIGS:
      handle_qpigs_(message);
      break;
    case POLLING_QMOD:
      handle_qmod_(message);
      break;
    case POLLING_QFLAG:
      handle_qflag_(message);
      break;
    case POLLING_QPIWS:
      handle_qpiws_(message);
      break;
    case POLLING_QT:
      handle_qt_(message);
      break;
    case POLLING_QMN:
      handle_qmn_(message);
      break;
    default:
      break;
  }
}
void Pipsolar::handle_poll_error_(ENUMPollingCommand polling_command) {
  // handlers are designed in a way that an empty message sets all sensors to unknown
  this->handle_poll_response_(polling_command, "");
}

void Pipsolar::handle_qpiri_(const char *message) {
  if (this->last_qpiri_) {
    this->last_qpiri_->publish_state(message);
  }

  size_t pos = 0;
  this->skip_start_(message, &pos);

  this->read_float_sensor_(message, &pos, this->grid_rating_voltage_);
  this->read_float_sensor_(message, &pos, this->grid_rating_current_);
  this->read_float_sensor_(message, &pos, this->ac_output_rating_voltage_);
  this->read_float_sensor_(message, &pos, this->ac_output_rating_frequency_);
  this->read_float_sensor_(message, &pos, this->ac_output_rating_current_);

  this->read_int_sensor_(message, &pos, this->ac_output_rating_apparent_power_);
  this->read_int_sensor_(message, &pos, this->ac_output_rating_active_power_);

  this->read_float_sensor_(message, &pos, this->battery_rating_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_recharge_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_under_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_bulk_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_float_voltage_);

  this->read_int_sensor_(message, &pos, this->battery_type_);
  this->read_int_sensor_(message, &pos, this->current_max_ac_charging_current_);
  this->read_int_sensor_(message, &pos, this->current_max_charging_current_);

  esphome::optional<int> input_voltage_range = parse_number<int32_t>(this->read_field_(message, &pos));
  esphome::optional<int> output_source_priority = parse_number<int32_t>(this->read_field_(message, &pos));

  this->read_int_sensor_(message, &pos, this->charger_source_priority_);
  this->read_int_sensor_(message, &pos, this->parallel_max_num_);
  this->read_int_sensor_(message, &pos, this->machine_type_);
  this->read_int_sensor_(message, &pos, this->topology_);
  this->read_int_sensor_(message, &pos, this->output_mode_);

  this->read_float_sensor_(message, &pos, this->battery_redischarge_voltage_);

  esphome::optional<int> pv_ok_condition_for_parallel = parse_number<int32_t>(this->read_field_(message, &pos));
  esphome::optional<int> pv_power_balance = parse_number<int32_t>(this->read_field_(message, &pos));

  if (this->input_voltage_range_) {
    this->input_voltage_range_->publish_state(input_voltage_range.value_or(NAN));
  }
  // special for input voltage range switch
  if (this->input_voltage_range_switch_ && input_voltage_range.has_value()) {
    this->input_voltage_range_switch_->publish_state(input_voltage_range.value() == 1);
  }

  if (this->output_source_priority_) {
    this->output_source_priority_->publish_state(output_source_priority.value_or(NAN));
  }
  // special for output source priority switches
  if (this->output_source_priority_utility_switch_ && output_source_priority.has_value()) {
    this->output_source_priority_utility_switch_->publish_state(output_source_priority.value() == 0);
  }
  if (this->output_source_priority_solar_switch_ && output_source_priority.has_value()) {
    this->output_source_priority_solar_switch_->publish_state(output_source_priority.value() == 1);
  }
  if (this->output_source_priority_battery_switch_ && output_source_priority.has_value()) {
    this->output_source_priority_battery_switch_->publish_state(output_source_priority.value() == 2);
  }
  if (this->output_source_priority_hybrid_switch_ && output_source_priority.has_value()) {
    this->output_source_priority_hybrid_switch_->publish_state(output_source_priority.value() == 3);
  }

  if (this->pv_ok_condition_for_parallel_) {
    this->pv_ok_condition_for_parallel_->publish_state(pv_ok_condition_for_parallel.value_or(NAN));
  }
  // special for pv ok condition switch
  if (this->pv_ok_condition_for_parallel_switch_ && pv_ok_condition_for_parallel.has_value()) {
    this->pv_ok_condition_for_parallel_switch_->publish_state(pv_ok_condition_for_parallel.value() == 1);
  }

  if (this->pv_power_balance_) {
    this->pv_power_balance_->publish_state(pv_power_balance.value_or(NAN));
  }
  // special for power balance switch
  if (this->pv_power_balance_switch_ && pv_power_balance.has_value()) {
    this->pv_power_balance_switch_->publish_state(pv_power_balance.value() == 1);
  }
}

void Pipsolar::handle_qpigs_(const char *message) {
  if (this->last_qpigs_) {
    this->last_qpigs_->publish_state(message);
  }

  size_t pos = 0;
  this->skip_start_(message, &pos);

  this->read_float_sensor_(message, &pos, this->grid_voltage_);
  this->read_float_sensor_(message, &pos, this->grid_frequency_);
  this->read_float_sensor_(message, &pos, this->ac_output_voltage_);
  this->read_float_sensor_(message, &pos, this->ac_output_frequency_);

  this->read_int_sensor_(message, &pos, this->ac_output_apparent_power_);
  this->read_int_sensor_(message, &pos, this->ac_output_active_power_);
  this->read_int_sensor_(message, &pos, this->output_load_percent_);
  this->read_int_sensor_(message, &pos, this->bus_voltage_);

  this->read_float_sensor_(message, &pos, this->battery_voltage_);

  this->read_int_sensor_(message, &pos, this->battery_charging_current_);
  this->read_int_sensor_(message, &pos, this->battery_capacity_percent_);
  this->read_int_sensor_(message, &pos, this->inverter_heat_sink_temperature_);

  this->read_float_sensor_(message, &pos, this->pv_input_current_for_battery_);
  this->read_float_sensor_(message, &pos, this->pv_input_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_voltage_scc_);

  this->read_int_sensor_(message, &pos, this->battery_discharge_current_);

  std::string device_status_1 = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 0), this->add_sbu_priority_version_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 1), this->configuration_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 2), this->scc_firmware_version_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 3), this->load_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 4), this->battery_voltage_to_steady_while_charging_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 5), this->charging_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 6), this->scc_charging_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 7), this->ac_charging_status_);

  esphome::optional<int> battery_voltage_offset_for_fans_on = parse_number<int32_t>(this->read_field_(message, &pos));
  if (this->battery_voltage_offset_for_fans_on_) {
    this->battery_voltage_offset_for_fans_on_->publish_state(battery_voltage_offset_for_fans_on.value_or(NAN) / 10.0f);
  }
  this->read_int_sensor_(message, &pos, this->eeprom_version_);
  this->read_int_sensor_(message, &pos, this->pv_charging_power_);

  std::string device_status_2 = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(device_status_2, 0), this->charging_to_floating_mode_);
  this->publish_binary_sensor_(this->get_bit_(device_status_2, 1), this->switch_on_);
  this->publish_binary_sensor_(this->get_bit_(device_status_2, 2), this->dustproof_installed_);
}

void Pipsolar::handle_qmod_(const char *message) {
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

void Pipsolar::handle_qflag_(const char *message) {
  // result like:"(EbkuvxzDajy"
  // get through all char: ignore first "(" Enable flag on 'E', Disable on 'D') else set the corresponding value
  if (this->last_qflag_) {
    this->last_qflag_->publish_state(message);
  }

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

  this->publish_binary_sensor_(values.silence_buzzer_open_buzzer, this->silence_buzzer_open_buzzer_);
  this->publish_binary_sensor_(values.overload_bypass_function, this->overload_bypass_function_);
  this->publish_binary_sensor_(values.lcd_escape_to_default, this->lcd_escape_to_default_);
  this->publish_binary_sensor_(values.overload_restart_function, this->overload_restart_function_);
  this->publish_binary_sensor_(values.over_temperature_restart_function, this->over_temperature_restart_function_);
  this->publish_binary_sensor_(values.backlight_on, this->backlight_on_);
  this->publish_binary_sensor_(values.alarm_on_when_primary_source_interrupt,
                               this->alarm_on_when_primary_source_interrupt_);
  this->publish_binary_sensor_(values.fault_code_record, this->fault_code_record_);
  this->publish_binary_sensor_(values.power_saving, this->power_saving_);
}

void Pipsolar::handle_qpiws_(const char *message) {
  // '(00000000000000000000000000000000'
  // iterate over all available flag (as not all models have all flags, but at least in the same order)
  if (this->last_qpiws_) {
    this->last_qpiws_->publish_state(message);
  }

  size_t pos = 0;
  this->skip_start_(message, &pos);
  std::string flags = this->read_field_(message, &pos);

  esphome::optional<bool> enabled;
  bool value_warnings_present = false;
  bool value_faults_present = false;

  for (size_t i = 0; i < 36; i++) {
    if (i == 31 || i == 32) {
      // special case for fault code
      continue;
    }
    enabled = this->get_bit_(flags, i);
    switch (i) {
      case 0:
        this->publish_binary_sensor_(enabled, this->warning_power_loss_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 1:
        this->publish_binary_sensor_(enabled, this->fault_inverter_fault_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 2:
        this->publish_binary_sensor_(enabled, this->fault_bus_over_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 3:
        this->publish_binary_sensor_(enabled, this->fault_bus_under_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 4:
        this->publish_binary_sensor_(enabled, this->fault_bus_soft_fail_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 5:
        this->publish_binary_sensor_(enabled, this->warning_line_fail_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 6:
        this->publish_binary_sensor_(enabled, this->fault_opvshort_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 7:
        this->publish_binary_sensor_(enabled, this->fault_inverter_voltage_too_low_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 8:
        this->publish_binary_sensor_(enabled, this->fault_inverter_voltage_too_high_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 9:
        this->publish_binary_sensor_(enabled, this->warning_over_temperature_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 10:
        this->publish_binary_sensor_(enabled, this->warning_fan_lock_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 11:
        this->publish_binary_sensor_(enabled, this->warning_battery_voltage_high_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 12:
        this->publish_binary_sensor_(enabled, this->warning_battery_low_alarm_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 14:
        this->publish_binary_sensor_(enabled, this->warning_battery_under_shutdown_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 15:
        this->publish_binary_sensor_(enabled, this->warning_battery_derating_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 16:
        this->publish_binary_sensor_(enabled, this->warning_over_load_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 17:
        this->publish_binary_sensor_(enabled, this->warning_eeprom_failed_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 18:
        this->publish_binary_sensor_(enabled, this->fault_inverter_over_current_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 19:
        this->publish_binary_sensor_(enabled, this->fault_inverter_soft_failed_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 20:
        this->publish_binary_sensor_(enabled, this->fault_self_test_failed_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 21:
        this->publish_binary_sensor_(enabled, this->fault_op_dc_voltage_over_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 22:
        this->publish_binary_sensor_(enabled, this->fault_battery_open_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 23:
        this->publish_binary_sensor_(enabled, this->fault_current_sensor_failed_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 24:
        this->publish_binary_sensor_(enabled, this->fault_battery_short_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 25:
        this->publish_binary_sensor_(enabled, this->warning_power_limit_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 26:
        this->publish_binary_sensor_(enabled, this->warning_pv_voltage_high_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 27:
        this->publish_binary_sensor_(enabled, this->fault_mppt_overload_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 28:
        this->publish_binary_sensor_(enabled, this->warning_mppt_overload_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 29:
        this->publish_binary_sensor_(enabled, this->warning_battery_too_low_to_charge_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 30:
        this->publish_binary_sensor_(enabled, this->fault_dc_dc_over_current_);
        value_faults_present |= enabled.value_or(false);
        break;
      case 33:
        this->publish_binary_sensor_(enabled, this->warning_low_pv_energy_);
        value_warnings_present |= enabled.value_or(false);
        break;
      case 34:
        this->publish_binary_sensor_(enabled, this->warning_high_ac_input_during_bus_soft_start_);
        value_warnings_present |= enabled.value_or(false);
      case 35:
        this->publish_binary_sensor_(enabled, this->warning_battery_equalization_);
        value_warnings_present |= enabled.value_or(false);
        break;
    }
  }

  this->publish_binary_sensor_(value_warnings_present, this->warnings_present_);
  this->publish_binary_sensor_(value_faults_present, this->faults_present_);

  if (this->fault_code_) {
    if (flags.length() < 33) {
      this->fault_code_->publish_state(NAN);
    } else {
      std::string fc(flags, 31, 2);
      this->fault_code_->publish_state(parse_number<int>(fc).value_or(NAN));
    }
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

void Pipsolar::skip_start_(const char *message, size_t *pos) {
  if (message[*pos] == '(') {
    (*pos)++;
  }
}
void Pipsolar::skip_field_(const char *message, size_t *pos) {
  // find delimiter or end of string
  while (message[*pos] != '\0' && message[*pos] != ' ') {
    (*pos)++;
  }
  if (message[*pos] != '\0') {
    // skip delimiter after this field if there is one
    (*pos)++;
  }
}
std::string Pipsolar::read_field_(const char *message, size_t *pos) {
  size_t begin = *pos;
  // find delimiter or end of string
  while (message[*pos] != '\0' && message[*pos] != ' ') {
    (*pos)++;
  }
  if (*pos == begin) {
    return "";
  }

  std::string field(message, begin, *pos - begin);

  if (message[*pos] != '\0') {
    // skip delimiter after this field if there is one
    (*pos)++;
  }

  return field;
}

void Pipsolar::read_float_sensor_(const char *message, size_t *pos, sensor::Sensor *sensor) {
  if (sensor != nullptr) {
    std::string field = this->read_field_(message, pos);
    sensor->publish_state(parse_number<float>(field).value_or(NAN));
  } else {
    this->skip_field_(message, pos);
  }
}
void Pipsolar::read_int_sensor_(const char *message, size_t *pos, sensor::Sensor *sensor) {
  if (sensor != nullptr) {
    std::string field = this->read_field_(message, pos);
    esphome::optional<int32_t> parsed = parse_number<int32_t>(field);
    sensor->publish_state(parsed.has_value() ? parsed.value() : NAN);
  } else {
    this->skip_field_(message, pos);
  }
}

void Pipsolar::publish_binary_sensor_(esphome::optional<bool> b, binary_sensor::BinarySensor *sensor) {
  if (sensor) {
    if (b.has_value()) {
      sensor->publish_state(b.value());
    } else {
      sensor->invalidate_state();
    }
  }
}

esphome::optional<bool> Pipsolar::get_bit_(std::string bits, uint8_t bit_pos) {
  if (bit_pos >= bits.length()) {
    return {};
  }
  return bits[bit_pos] == '1';
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
