#include "ezo_pmp.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ezo_pmp {

static const char *const TAG = "ezo-pmp";

static const uint16_t EZO_PMP_COMMAND_NONE = 0;
static const uint16_t EZO_PMP_COMMAND_TYPE_READ = 1;

static const uint16_t EZO_PMP_COMMAND_FIND = 2;
static const uint16_t EZO_PMP_COMMAND_DOSE_CONTINUOUSLY = 4;
static const uint16_t EZO_PMP_COMMAND_DOSE_VOLUME = 8;
static const uint16_t EZO_PMP_COMMAND_DOSE_VOLUME_OVER_TIME = 16;
static const uint16_t EZO_PMP_COMMAND_DOSE_WITH_CONSTANT_FLOW_RATE = 32;
static const uint16_t EZO_PMP_COMMAND_SET_CALIBRATION_VOLUME = 64;
static const uint16_t EZO_PMP_COMMAND_CLEAR_TOTAL_VOLUME_DOSED = 128;
static const uint16_t EZO_PMP_COMMAND_CLEAR_CALIBRATION = 256;
static const uint16_t EZO_PMP_COMMAND_PAUSE_DOSING = 512;
static const uint16_t EZO_PMP_COMMAND_STOP_DOSING = 1024;
static const uint16_t EZO_PMP_COMMAND_CHANGE_I2C_ADDRESS = 2048;
static const uint16_t EZO_PMP_COMMAND_EXEC_ARBITRARY_COMMAND_ADDRESS = 4096;

static const uint16_t EZO_PMP_COMMAND_READ_DOSING = 3;
static const uint16_t EZO_PMP_COMMAND_READ_SINGLE_REPORT = 5;
static const uint16_t EZO_PMP_COMMAND_READ_MAX_FLOW_RATE = 9;
static const uint16_t EZO_PMP_COMMAND_READ_PAUSE_STATUS = 17;
static const uint16_t EZO_PMP_COMMAND_READ_TOTAL_VOLUME_DOSED = 33;
static const uint16_t EZO_PMP_COMMAND_READ_ABSOLUTE_TOTAL_VOLUME_DOSED = 65;
static const uint16_t EZO_PMP_COMMAND_READ_CALIBRATION_STATUS = 129;
static const uint16_t EZO_PMP_COMMAND_READ_PUMP_VOLTAGE = 257;

static const std::string DOSING_MODE_NONE = "None";
static const std::string DOSING_MODE_VOLUME = "Volume";
static const std::string DOSING_MODE_VOLUME_OVER_TIME = "Volume/Time";
static const std::string DOSING_MODE_CONSTANT_FLOW_RATE = "Constant Flow Rate";
static const std::string DOSING_MODE_CONTINUOUS = "Continuous";

void EzoPMP::dump_config() {
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with EZO-PMP circuit failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void EzoPMP::update() {
  if (this->is_waiting_) {
    return;
  }

  if (this->is_first_read_) {
    this->queue_command_(EZO_PMP_COMMAND_READ_CALIBRATION_STATUS, 0, 0, (bool) this->calibration_status_);
    this->queue_command_(EZO_PMP_COMMAND_READ_MAX_FLOW_RATE, 0, 0, (bool) this->max_flow_rate_);
    this->queue_command_(EZO_PMP_COMMAND_READ_SINGLE_REPORT, 0, 0, (bool) this->current_volume_dosed_);
    this->queue_command_(EZO_PMP_COMMAND_READ_TOTAL_VOLUME_DOSED, 0, 0, (bool) this->total_volume_dosed_);
    this->queue_command_(EZO_PMP_COMMAND_READ_ABSOLUTE_TOTAL_VOLUME_DOSED, 0, 0,
                         (bool) this->absolute_total_volume_dosed_);
    this->queue_command_(EZO_PMP_COMMAND_READ_PAUSE_STATUS, 0, 0, true);
    this->is_first_read_ = false;
  }

  if (!this->is_waiting_ && this->peek_next_command_() == EZO_PMP_COMMAND_NONE) {
    this->queue_command_(EZO_PMP_COMMAND_READ_DOSING, 0, 0, true);

    if (this->is_dosing_flag_) {
      this->queue_command_(EZO_PMP_COMMAND_READ_SINGLE_REPORT, 0, 0, (bool) this->current_volume_dosed_);
      this->queue_command_(EZO_PMP_COMMAND_READ_TOTAL_VOLUME_DOSED, 0, 0, (bool) this->total_volume_dosed_);
      this->queue_command_(EZO_PMP_COMMAND_READ_ABSOLUTE_TOTAL_VOLUME_DOSED, 0, 0,
                           (bool) this->absolute_total_volume_dosed_);
    }

    this->queue_command_(EZO_PMP_COMMAND_READ_PUMP_VOLTAGE, 0, 0, (bool) this->pump_voltage_);
  } else {
    ESP_LOGV(TAG, "Not Scheduling new Command during update()");
  }
}

void EzoPMP::loop() {
  // If we are not waiting for anything and there is no command to be sent, return
  if (!this->is_waiting_ && this->peek_next_command_() == EZO_PMP_COMMAND_NONE) {
    return;
  }

  // If we are not waiting for anything and there IS a command to be sent, do it.
  if (!this->is_waiting_ && this->peek_next_command_() != EZO_PMP_COMMAND_NONE) {
    this->send_next_command_();
  }

  // If we are waiting for something but it isn't ready yet, then return
  if (this->is_waiting_ && millis() - this->start_time_ < this->wait_time_) {
    return;
  }

  // We are waiting for something and it should be ready.
  this->read_command_result_();
}

void EzoPMP::clear_current_command_() {
  this->current_command_ = EZO_PMP_COMMAND_NONE;
  this->is_waiting_ = false;
}

void EzoPMP::read_command_result_() {
  uint8_t response_buffer[21] = {'\0'};

  response_buffer[0] = 0;
  if (!this->read_bytes_raw(response_buffer, 20)) {
    ESP_LOGE(TAG, "read error");
    this->clear_current_command_();
    return;
  }

  switch (response_buffer[0]) {
    case 254:
      return;  // keep waiting
    case 1:
      break;
    case 2:
      ESP_LOGE(TAG, "device returned a syntax error");
      this->clear_current_command_();
      return;
    case 255:
      ESP_LOGE(TAG, "device returned no data");
      this->clear_current_command_();
      return;
    default:
      ESP_LOGE(TAG, "device returned an unknown response: %d", response_buffer[0]);
      this->clear_current_command_();
      return;
  }

  char first_parameter_buffer[10] = {'\0'};
  char second_parameter_buffer[10] = {'\0'};
  char third_parameter_buffer[10] = {'\0'};

  first_parameter_buffer[0] = '\0';
  second_parameter_buffer[0] = '\0';
  third_parameter_buffer[0] = '\0';

  int current_parameter = 1;

  size_t position_in_parameter_buffer = 0;
  // some sensors return multiple comma-separated values, terminate string after first one
  for (size_t i = 1; i < sizeof(response_buffer) - 1; i++) {
    char current_char = response_buffer[i];

    if (current_char == '\0') {
      ESP_LOGV(TAG, "Read Response from device: %s", (char *) response_buffer);
      ESP_LOGV(TAG, "First Component: %s", (char *) first_parameter_buffer);
      ESP_LOGV(TAG, "Second Component: %s", (char *) second_parameter_buffer);
      ESP_LOGV(TAG, "Third Component: %s", (char *) third_parameter_buffer);

      break;
    }

    if (current_char == ',') {
      current_parameter++;
      position_in_parameter_buffer = 0;
      continue;
    }

    switch (current_parameter) {
      case 1:
        first_parameter_buffer[position_in_parameter_buffer] = current_char;
        first_parameter_buffer[position_in_parameter_buffer + 1] = '\0';
        break;
      case 2:
        second_parameter_buffer[position_in_parameter_buffer] = current_char;
        second_parameter_buffer[position_in_parameter_buffer + 1] = '\0';
        break;
      case 3:
        third_parameter_buffer[position_in_parameter_buffer] = current_char;
        third_parameter_buffer[position_in_parameter_buffer + 1] = '\0';
        break;
    }

    position_in_parameter_buffer++;
  }

  auto parsed_first_parameter = parse_number<float>(first_parameter_buffer);
  auto parsed_second_parameter = parse_number<float>(second_parameter_buffer);
  auto parsed_third_parameter = parse_number<float>(third_parameter_buffer);

  switch (this->current_command_) {
    // Read Commands
    case EZO_PMP_COMMAND_READ_DOSING:  // Page 54
      if (parsed_third_parameter.has_value())
        this->is_dosing_flag_ = parsed_third_parameter.value_or(0) == 1;

      if (this->is_dosing_)
        this->is_dosing_->publish_state(this->is_dosing_flag_);

      if (parsed_second_parameter.has_value() && this->last_volume_requested_) {
        this->last_volume_requested_->publish_state(parsed_second_parameter.value_or(0));
      }

      if (!this->is_dosing_flag_ && !this->is_paused_flag_) {
        // If pump is not paused and not dispensing
        if (this->dosing_mode_ && this->dosing_mode_->state != DOSING_MODE_NONE)
          this->dosing_mode_->publish_state(DOSING_MODE_NONE);
      }

      break;

    case EZO_PMP_COMMAND_READ_SINGLE_REPORT:  // Single Report (page 53)
      if (parsed_first_parameter.has_value() && (bool) this->current_volume_dosed_) {
        this->current_volume_dosed_->publish_state(parsed_first_parameter.value_or(0));
      }
      break;

    case EZO_PMP_COMMAND_READ_MAX_FLOW_RATE:  // Constant Flow Rate (page 57)
      if (parsed_second_parameter.has_value() && this->max_flow_rate_)
        this->max_flow_rate_->publish_state(parsed_second_parameter.value_or(0));
      break;

    case EZO_PMP_COMMAND_READ_PAUSE_STATUS:  // Pause (page 61)
      if (parsed_second_parameter.has_value())
        this->is_paused_flag_ = parsed_second_parameter.value_or(0) == 1;

      if (this->is_paused_)
        this->is_paused_->publish_state(this->is_paused_flag_);
      break;

    case EZO_PMP_COMMAND_READ_TOTAL_VOLUME_DOSED:  // Total Volume Dispensed (page 64)
      if (parsed_second_parameter.has_value() && this->total_volume_dosed_)
        this->total_volume_dosed_->publish_state(parsed_second_parameter.value_or(0));
      break;

    case EZO_PMP_COMMAND_READ_ABSOLUTE_TOTAL_VOLUME_DOSED:  // Total Volume Dispensed (page 64)
      if (parsed_second_parameter.has_value() && this->absolute_total_volume_dosed_)
        this->absolute_total_volume_dosed_->publish_state(parsed_second_parameter.value_or(0));
      break;

    case EZO_PMP_COMMAND_READ_CALIBRATION_STATUS:  // Calibration (page 65)
      if (parsed_second_parameter.has_value() && this->calibration_status_) {
        if (parsed_second_parameter.value_or(0) == 1) {
          this->calibration_status_->publish_state("Fixed Volume");
        } else if (parsed_second_parameter.value_or(0) == 2) {
          this->calibration_status_->publish_state("Volume/Time");
        } else if (parsed_second_parameter.value_or(0) == 3) {
          this->calibration_status_->publish_state("Fixed Volume & Volume/Time");
        } else {
          this->calibration_status_->publish_state("Uncalibrated");
        }
      }
      break;

    case EZO_PMP_COMMAND_READ_PUMP_VOLTAGE:  // Pump Voltage (page 67)
      if (parsed_second_parameter.has_value() && this->pump_voltage_)
        this->pump_voltage_->publish_state(parsed_second_parameter.value_or(0));
      break;

      // Non-Read Commands

    case EZO_PMP_COMMAND_DOSE_VOLUME:  // Volume Dispensing (page 55)
      if (this->dosing_mode_ && this->dosing_mode_->state != DOSING_MODE_VOLUME)
        this->dosing_mode_->publish_state(DOSING_MODE_VOLUME);
      break;

    case EZO_PMP_COMMAND_DOSE_VOLUME_OVER_TIME:  // Dose over time (page 56)
      if (this->dosing_mode_ && this->dosing_mode_->state != DOSING_MODE_VOLUME_OVER_TIME)
        this->dosing_mode_->publish_state(DOSING_MODE_VOLUME_OVER_TIME);
      break;

    case EZO_PMP_COMMAND_DOSE_WITH_CONSTANT_FLOW_RATE:  // Constant Flow Rate (page 57)
      if (this->dosing_mode_ && this->dosing_mode_->state != DOSING_MODE_CONSTANT_FLOW_RATE)
        this->dosing_mode_->publish_state(DOSING_MODE_CONSTANT_FLOW_RATE);
      break;

    case EZO_PMP_COMMAND_DOSE_CONTINUOUSLY:  // Continuous Dispensing (page 54)
      if (this->dosing_mode_ && this->dosing_mode_->state != DOSING_MODE_CONTINUOUS)
        this->dosing_mode_->publish_state(DOSING_MODE_CONTINUOUS);
      break;

    case EZO_PMP_COMMAND_STOP_DOSING:  // Stop (page 62)
      this->is_paused_flag_ = false;
      if (this->is_paused_)
        this->is_paused_->publish_state(this->is_paused_flag_);
      if (this->dosing_mode_ && this->dosing_mode_->state != DOSING_MODE_NONE)
        this->dosing_mode_->publish_state(DOSING_MODE_NONE);
      break;

    case EZO_PMP_COMMAND_EXEC_ARBITRARY_COMMAND_ADDRESS:
      ESP_LOGI(TAG, "Arbitrary Command Response: %s", (char *) response_buffer);
      break;

    case EZO_PMP_COMMAND_CLEAR_CALIBRATION:         // Clear Calibration (page 65)
    case EZO_PMP_COMMAND_PAUSE_DOSING:              // Pause (page 61)
    case EZO_PMP_COMMAND_SET_CALIBRATION_VOLUME:    // Set Calibration Volume (page 65)
    case EZO_PMP_COMMAND_CLEAR_TOTAL_VOLUME_DOSED:  // Clear Total Volume Dosed (page 64)
    case EZO_PMP_COMMAND_FIND:                      // Find (page 52)
      // Nothing to do here
      break;

    case EZO_PMP_COMMAND_TYPE_READ:
    case EZO_PMP_COMMAND_NONE:
    default:
      ESP_LOGE(TAG, "Unsupported command received: %d", this->current_command_);
      return;
  }

  this->clear_current_command_();
}

void EzoPMP::send_next_command_() {
  int wait_time_for_command = 400;  // milliseconds
  uint8_t command_buffer[21];
  int command_buffer_length = 0;

  this->pop_next_command_();  // this->next_command will be updated.

  switch (this->next_command_) {
    // Read Commands
    case EZO_PMP_COMMAND_READ_DOSING:  // Page 54
      command_buffer_length = sprintf((char *) command_buffer, "D,?");
      break;

    case EZO_PMP_COMMAND_READ_SINGLE_REPORT:  // Single Report (page 53)
      command_buffer_length = sprintf((char *) command_buffer, "R");
      break;

    case EZO_PMP_COMMAND_READ_MAX_FLOW_RATE:
      command_buffer_length = sprintf((char *) command_buffer, "DC,?");
      break;

    case EZO_PMP_COMMAND_READ_PAUSE_STATUS:
      command_buffer_length = sprintf((char *) command_buffer, "P,?");
      break;

    case EZO_PMP_COMMAND_READ_TOTAL_VOLUME_DOSED:
      command_buffer_length = sprintf((char *) command_buffer, "TV,?");
      break;

    case EZO_PMP_COMMAND_READ_ABSOLUTE_TOTAL_VOLUME_DOSED:
      command_buffer_length = sprintf((char *) command_buffer, "ATV,?");
      break;

    case EZO_PMP_COMMAND_READ_CALIBRATION_STATUS:
      command_buffer_length = sprintf((char *) command_buffer, "Cal,?");
      break;

    case EZO_PMP_COMMAND_READ_PUMP_VOLTAGE:
      command_buffer_length = sprintf((char *) command_buffer, "PV,?");
      break;

      // Non-Read Commands

    case EZO_PMP_COMMAND_FIND:  // Find (page 52)
      command_buffer_length = sprintf((char *) command_buffer, "Find");
      wait_time_for_command = 60000;  // This command will block all updates for a minute
      break;

    case EZO_PMP_COMMAND_DOSE_CONTINUOUSLY:  // Continuous Dispensing (page 54)
      command_buffer_length = sprintf((char *) command_buffer, "D,*");
      break;

    case EZO_PMP_COMMAND_CLEAR_TOTAL_VOLUME_DOSED:  // Clear Total Volume Dosed (page 64)
      command_buffer_length = sprintf((char *) command_buffer, "Clear");
      break;

    case EZO_PMP_COMMAND_CLEAR_CALIBRATION:  // Clear Calibration (page 65)
      command_buffer_length = sprintf((char *) command_buffer, "Cal,clear");
      break;

    case EZO_PMP_COMMAND_PAUSE_DOSING:  // Pause (page 61)
      command_buffer_length = sprintf((char *) command_buffer, "P");
      break;

    case EZO_PMP_COMMAND_STOP_DOSING:  // Stop (page 62)
      command_buffer_length = sprintf((char *) command_buffer, "X");
      break;

      // Non-Read commands with parameters

    case EZO_PMP_COMMAND_DOSE_VOLUME:  // Volume Dispensing (page 55)
      command_buffer_length = sprintf((char *) command_buffer, "D,%0.1f", this->next_command_volume_);
      break;

    case EZO_PMP_COMMAND_DOSE_VOLUME_OVER_TIME:  // Dose over time (page 56)
      command_buffer_length =
          sprintf((char *) command_buffer, "D,%0.1f,%i", this->next_command_volume_, this->next_command_duration_);
      break;

    case EZO_PMP_COMMAND_DOSE_WITH_CONSTANT_FLOW_RATE:  // Constant Flow Rate (page 57)
      command_buffer_length =
          sprintf((char *) command_buffer, "DC,%0.1f,%i", this->next_command_volume_, this->next_command_duration_);
      break;

    case EZO_PMP_COMMAND_SET_CALIBRATION_VOLUME:  // Set Calibration Volume (page 65)
      command_buffer_length = sprintf((char *) command_buffer, "Cal,%0.2f", this->next_command_volume_);
      break;

    case EZO_PMP_COMMAND_CHANGE_I2C_ADDRESS:  // Change I2C Address (page 73)
      command_buffer_length = sprintf((char *) command_buffer, "I2C,%i", this->next_command_duration_);
      break;

    case EZO_PMP_COMMAND_EXEC_ARBITRARY_COMMAND_ADDRESS:  // Run an arbitrary command
      command_buffer_length = sprintf((char *) command_buffer, this->arbitrary_command_, this->next_command_duration_);
      ESP_LOGI(TAG, "Sending arbitrary command: %s", (char *) command_buffer);
      break;

    case EZO_PMP_COMMAND_TYPE_READ:
    case EZO_PMP_COMMAND_NONE:
    default:
      ESP_LOGE(TAG, "Unsupported command received: %d", this->next_command_);
      return;
  }

  // Send command
  ESP_LOGV(TAG, "Sending command to device: %s", (char *) command_buffer);
  this->write(command_buffer, command_buffer_length);

  this->current_command_ = this->next_command_;
  this->next_command_ = EZO_PMP_COMMAND_NONE;
  this->is_waiting_ = true;
  this->start_time_ = millis();
  this->wait_time_ = wait_time_for_command;
}

void EzoPMP::pop_next_command_() {
  if (this->next_command_queue_length_ <= 0) {
    ESP_LOGE(TAG, "Tried to dequeue command from empty queue");
    this->next_command_ = EZO_PMP_COMMAND_NONE;
    this->next_command_volume_ = 0;
    this->next_command_duration_ = 0;
    return;
  }

  // Read from Head
  this->next_command_ = this->next_command_queue_[this->next_command_queue_head_];
  this->next_command_volume_ = this->next_command_volume_queue_[this->next_command_queue_head_];
  this->next_command_duration_ = this->next_command_duration_queue_[this->next_command_queue_head_];

  // Move positions
  next_command_queue_head_++;
  if (next_command_queue_head_ >= 10) {
    next_command_queue_head_ = 0;
  }

  next_command_queue_length_--;
}

uint16_t EzoPMP::peek_next_command_() {
  if (this->next_command_queue_length_ <= 0) {
    return EZO_PMP_COMMAND_NONE;
  }

  return this->next_command_queue_[this->next_command_queue_head_];
}

void EzoPMP::queue_command_(uint16_t command, double volume, int duration, bool should_schedule) {
  if (!should_schedule) {
    return;
  }

  if (this->next_command_queue_length_ >= 10) {
    ESP_LOGE(TAG, "Tried to queue command '%d' but queue is full", command);
    return;
  }

  this->next_command_queue_[this->next_command_queue_last_] = command;
  this->next_command_volume_queue_[this->next_command_queue_last_] = volume;
  this->next_command_duration_queue_[this->next_command_queue_last_] = duration;

  ESP_LOGV(TAG, "Queue command '%d' in position '%d'", command, next_command_queue_last_);

  // Move positions
  next_command_queue_last_++;
  if (next_command_queue_last_ >= 10) {
    next_command_queue_last_ = 0;
  }

  next_command_queue_length_++;
}

// Actions

void EzoPMP::find() { this->queue_command_(EZO_PMP_COMMAND_FIND, 0, 0, true); }

void EzoPMP::dose_continuously() {
  this->queue_command_(EZO_PMP_COMMAND_DOSE_CONTINUOUSLY, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_DOSING, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_SINGLE_REPORT, 0, 0, (bool) this->current_volume_dosed_);
}

void EzoPMP::dose_volume(double volume) {
  this->queue_command_(EZO_PMP_COMMAND_DOSE_VOLUME, volume, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_DOSING, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_SINGLE_REPORT, 0, 0, (bool) this->current_volume_dosed_);
}

void EzoPMP::dose_volume_over_time(double volume, int duration) {
  this->queue_command_(EZO_PMP_COMMAND_DOSE_VOLUME_OVER_TIME, volume, duration, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_DOSING, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_SINGLE_REPORT, 0, 0, (bool) this->current_volume_dosed_);
}

void EzoPMP::dose_with_constant_flow_rate(double volume, int duration) {
  this->queue_command_(EZO_PMP_COMMAND_DOSE_WITH_CONSTANT_FLOW_RATE, volume, duration, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_DOSING, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_SINGLE_REPORT, 0, 0, (bool) this->current_volume_dosed_);
}

void EzoPMP::set_calibration_volume(double volume) {
  this->queue_command_(EZO_PMP_COMMAND_SET_CALIBRATION_VOLUME, volume, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_CALIBRATION_STATUS, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_MAX_FLOW_RATE, 0, 0, true);
}

void EzoPMP::clear_total_volume_dosed() {
  this->queue_command_(EZO_PMP_COMMAND_CLEAR_TOTAL_VOLUME_DOSED, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_SINGLE_REPORT, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_TOTAL_VOLUME_DOSED, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_ABSOLUTE_TOTAL_VOLUME_DOSED, 0, 0, true);
}

void EzoPMP::clear_calibration() {
  this->queue_command_(EZO_PMP_COMMAND_CLEAR_CALIBRATION, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_CALIBRATION_STATUS, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_MAX_FLOW_RATE, 0, 0, true);
}

void EzoPMP::pause_dosing() {
  this->queue_command_(EZO_PMP_COMMAND_PAUSE_DOSING, 0, 0, true);
  this->queue_command_(EZO_PMP_COMMAND_READ_PAUSE_STATUS, 0, 0, true);
}

void EzoPMP::stop_dosing() { this->queue_command_(EZO_PMP_COMMAND_STOP_DOSING, 0, 0, true); }

void EzoPMP::change_i2c_address(int address) {
  this->queue_command_(EZO_PMP_COMMAND_CHANGE_I2C_ADDRESS, 0, address, true);
}

void EzoPMP::exec_arbitrary_command(const std::basic_string<char> &command) {
  this->arbitrary_command_ = command.c_str();
  this->queue_command_(EZO_PMP_COMMAND_EXEC_ARBITRARY_COMMAND_ADDRESS, 0, 0, true);
}

}  // namespace ezo_pmp
}  // namespace esphome
