#include "ezo.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/progmem.h"

namespace esphome::ezo {

PROGMEM_STRING_TABLE(EzoCommandTypeStrings, "EZO_READ", "EZO_LED", "EZO_DEVICE_INFORMATION", "EZO_SLOPE",
                     "EZO_CALIBRATION", "EZO_SLEEP", "EZO_I2C", "EZO_T", "EZO_CUSTOM");

PROGMEM_STRING_TABLE(EzoCalibrationTypeStrings, "LOW", "MID", "HIGH");

void EZOSensor::dump_config() {
  LOG_SENSOR("", "EZO", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

static constexpr uint16_t READ_DELAY_MS = 900;
static constexpr size_t RESPONSE_SIZE = 32;

void EZOSensor::update() { this->get_state(); }

void EZOSensor::send_next_() {
  const bool after_read = this->state_ == EzoState::EZO_STATE_IDLE_AFTER_READ;
  // A read goes right after the command in flight, but never twice in a row while others wait.
  if (this->read_pending_ && (!after_read || this->queue_.empty())) {
    ESP_LOGVV(TAG, "Sending command \"R\"");
    static constexpr uint8_t READ_COMMAND = 'R';
    this->write(&READ_COMMAND, 1);
    this->read_pending_ = false;
    this->state_ = EzoState::EZO_STATE_SENT_READ;
    this->start_time_ = App.get_loop_component_start_time();
    return;
  }
  if (this->queue_.empty()) {
    this->disable_loop();
    return;
  }

  const EzoCommand &command = this->queue_.front();
  ESP_LOGVV(TAG, "Sending command \"%s\"", command.command);
  this->write(reinterpret_cast<const uint8_t *>(command.command), strlen(command.command));

  if (command.command_type == EzoCommandType::EZO_SLEEP ||
      command.command_type == EzoCommandType::EZO_I2C) {  // Commands with no return data
    if (command.command_type == EzoCommandType::EZO_I2C) {
      // The target is read back from "I2C,<n>" so several queued address changes each apply their own
      if (auto address = parse_number<uint8_t>(command.command + 4); address.has_value())
        this->address_ = *address;
    }
    this->queue_.pop();
    this->state_ = EzoState::EZO_STATE_IDLE;
    return;
  }

  this->state_ = EzoState::EZO_STATE_SENT_QUEUED;
  this->start_time_ = App.get_loop_component_start_time();
}

void EZOSensor::finish_command_() {
  if (this->state_ == EzoState::EZO_STATE_SENT_READ) {
    this->state_ = EzoState::EZO_STATE_IDLE_AFTER_READ;
  } else {
    this->queue_.pop();
    this->state_ = EzoState::EZO_STATE_IDLE;
  }
}

void EZOSensor::loop() {
  if (this->state_ == EzoState::EZO_STATE_IDLE || this->state_ == EzoState::EZO_STATE_IDLE_AFTER_READ) {
    this->send_next_();
    return;
  }

  const bool is_read = this->state_ == EzoState::EZO_STATE_SENT_READ;
  const uint16_t delay_ms = is_read ? READ_DELAY_MS : this->queue_.front().delay_ms;
  if (App.get_loop_component_start_time() - this->start_time_ < delay_ms)
    return;

  // One spare byte keeps the reply terminated when the device fills all 32.
  uint8_t buf[RESPONSE_SIZE + 1];
  buf[0] = 0;
  buf[RESPONSE_SIZE] = 0;

  if (!this->read_bytes_raw(buf, RESPONSE_SIZE)) {
    ESP_LOGE(TAG, "read error");
    this->finish_command_();
    return;
  }

  switch (buf[0]) {
    case 1:
      break;
    case 2:
      ESP_LOGE(TAG, "device returned a syntax error");
      break;
    case 254:
      return;  // keep waiting
    case 255:
      ESP_LOGE(TAG, "device returned no data");
      break;
    default:
      ESP_LOGE(TAG, "device returned an unknown response: %d", buf[0]);
      break;
  }

  const EzoCommandType command_type = is_read ? EzoCommandType::EZO_READ : this->queue_.front().command_type;
  ESP_LOGV(TAG, "Received buffer \"%s\" for command type %s", &buf[1],
           LOG_STR_ARG(EzoCommandTypeStrings::get_log_str(command_type, EzoCommandTypeStrings::LAST_INDEX)));

  char *payload = reinterpret_cast<char *>(&buf[1]);
  if (buf[0] == 1 && payload[0] != '\0') {
    char *comma = strchr(payload, ',');
    switch (command_type) {
      case EzoCommandType::EZO_READ: {
        // some sensors return multiple comma-separated values, terminate string after first one
        if (comma != nullptr)
          *comma = '\0';
        auto val = parse_number<float>(payload);
        if (!val.has_value()) {
          ESP_LOGW(TAG, "Can't convert '%s' to number!", payload);
        } else {
          this->publish_state(*val);
        }
        break;
      }
      case EzoCommandType::EZO_LED:
        this->led_callback_.call(payload[strlen(payload) - 1] == '1');
        break;
      case EzoCommandType::EZO_DEVICE_INFORMATION:
        if (comma != nullptr)
          this->device_infomation_callback_.call(std::string(comma + 1));
        break;
      case EzoCommandType::EZO_SLOPE:
        if (comma != nullptr)
          this->slope_callback_.call(std::string(comma + 1));
        break;
      case EzoCommandType::EZO_CALIBRATION:
        if (comma != nullptr)
          this->calibration_callback_.call(std::string(comma + 1));
        break;
      case EzoCommandType::EZO_T:
        if (comma != nullptr)
          this->t_callback_.call(std::string(comma + 1));
        break;
      case EzoCommandType::EZO_CUSTOM:
        this->custom_callback_.call(std::string(payload));
        break;
      default:
        break;
    }
  }
  this->finish_command_();
}

void EZOSensor::add_command_(const char *command, EzoCommandType command_type, uint16_t delay_ms) {
  const size_t length = strlen(command);
  if (length > EZO_MAX_COMMAND_LENGTH) {
    ESP_LOGW(TAG, "Command too long (%u > %u), dropped", static_cast<unsigned>(length),
             static_cast<unsigned>(EZO_MAX_COMMAND_LENGTH));
    return;
  }
  EzoCommand entry;
  memcpy(entry.command, command, length + 1);
  entry.delay_ms = delay_ms;
  entry.command_type = command_type;
  if (!this->queue_.push(entry)) {
    ESP_LOGW(TAG, "Command queue full, dropped \"%s\"", command);
    return;
  }
  this->enable_loop();
}

void EZOSensor::set_calibration_point_(EzoCalibrationType type, float value) {
  // max 21: "Cal,"(4) + type(4) + ","(1) + float(11) + null; use 24 for safety
  char payload[24];
  ESPHOME_snprintf_P(payload, sizeof(payload), ESPHOME_PSTR("Cal,%s,%0.2f"),
                     LOG_STR_ARG(EzoCalibrationTypeStrings::get_log_str(type, EzoCalibrationTypeStrings::LAST_INDEX)),
                     value);
  this->add_command_(payload, EzoCommandType::EZO_CALIBRATION, 900);
}

void EZOSensor::set_address(uint8_t address) {
  if (address > 0 && address < 128) {
    // max 8: "I2C,"(4) + uint8(3) + null
    char payload[8];
    ESPHOME_snprintf_P(payload, sizeof(payload), ESPHOME_PSTR("I2C,%u"), address);
    this->add_command_(payload, EzoCommandType::EZO_I2C);
  } else {
    ESP_LOGE(TAG, "Invalid I2C address");
  }
}

void EZOSensor::get_device_information() { this->add_command_("i", EzoCommandType::EZO_DEVICE_INFORMATION); }

void EZOSensor::set_sleep() { this->add_command_("Sleep", EzoCommandType::EZO_SLEEP); }

void EZOSensor::get_state() {
  this->read_pending_ = true;
  this->enable_loop();
}

void EZOSensor::get_slope() { this->add_command_("Slope,?", EzoCommandType::EZO_SLOPE); }

void EZOSensor::get_t() { this->add_command_("T,?", EzoCommandType::EZO_T); }

void EZOSensor::set_t(float value) {
  // max 14 bytes: "T,"(2) + float with "%0.2f" (up to 11 chars) + null(1); use 16 for alignment
  char payload[16];
  ESPHOME_snprintf_P(payload, sizeof(payload), ESPHOME_PSTR("T,%0.2f"), value);
  this->add_command_(payload, EzoCommandType::EZO_T);
}

void EZOSensor::set_tempcomp_value(float temp) { this->set_t(temp); }

void EZOSensor::get_calibration() { this->add_command_("Cal,?", EzoCommandType::EZO_CALIBRATION); }

void EZOSensor::set_calibration_point_low(float value) {
  this->set_calibration_point_(EzoCalibrationType::EZO_CAL_LOW, value);
}

void EZOSensor::set_calibration_point_mid(float value) {
  this->set_calibration_point_(EzoCalibrationType::EZO_CAL_MID, value);
}

void EZOSensor::set_calibration_point_high(float value) {
  this->set_calibration_point_(EzoCalibrationType::EZO_CAL_HIGH, value);
}

void EZOSensor::set_calibration_generic(float value) {
  // exact 16 bytes: "Cal," (4) + float with "%0.2f" (up to 11 chars, e.g. "-9999999.99") + null (1) = 16
  char payload[16];
  ESPHOME_snprintf_P(payload, sizeof(payload), ESPHOME_PSTR("Cal,%0.2f"), value);
  this->add_command_(payload, EzoCommandType::EZO_CALIBRATION, 900);
}

void EZOSensor::clear_calibration() { this->add_command_("Cal,clear", EzoCommandType::EZO_CALIBRATION); }

void EZOSensor::get_led_state() { this->add_command_("L,?", EzoCommandType::EZO_LED); }

void EZOSensor::set_led_state(bool on) { this->add_command_(on ? "L,1" : "L,0", EzoCommandType::EZO_LED); }

void EZOSensor::send_custom(const std::string &to_send) {
  this->add_command_(to_send.c_str(), EzoCommandType::EZO_CUSTOM);
}

}  // namespace esphome::ezo
