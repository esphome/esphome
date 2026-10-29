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

void EZOSensor::update() { this->get_state(); }

bool EZOSensor::next_command_() {
  // A read goes right after the command in flight, but never twice in a row while others wait.
  if (this->read_pending_ && (!this->last_was_read_ || this->queue_count_ == 0)) {
    this->current_.command[0] = 'R';
    this->current_.command[1] = '\0';
    this->current_.command_type = EzoCommandType::EZO_READ;
    this->current_.delay_ms = 900;
    this->read_pending_ = false;
    this->last_was_read_ = true;
  } else if (this->queue_count_ != 0) {
    this->current_ = this->queue_[this->queue_head_];
    this->queue_head_ = (this->queue_head_ + 1) % EZO_COMMAND_QUEUE_SIZE;
    this->queue_count_--;
    this->last_was_read_ = false;
  } else {
    return false;
  }
  this->current_.command_sent = false;
  this->busy_ = true;
  return true;
}

void EZOSensor::loop() {
  if (!this->busy_ && !this->next_command_()) {
    this->disable_loop();
    return;
  }

  EzoCommand *to_run = &this->current_;

  if (!to_run->command_sent) {
    ESP_LOGVV(TAG, "Sending command \"%s\"", to_run->command);

    this->write(reinterpret_cast<const uint8_t *>(to_run->command), strlen(to_run->command));

    if (to_run->command_type == EzoCommandType::EZO_SLEEP ||
        to_run->command_type == EzoCommandType::EZO_I2C) {  // Commands with no return data
      this->busy_ = false;
      if (to_run->command_type == EzoCommandType::EZO_I2C)
        this->address_ = this->new_address_;
      return;
    }

    this->start_time_ = App.get_loop_component_start_time();
    to_run->command_sent = true;
    return;
  }

  if (App.get_loop_component_start_time() - this->start_time_ < to_run->delay_ms)
    return;

  uint8_t buf[32];

  buf[0] = 0;

  if (!this->read_bytes_raw(buf, 32)) {
    ESP_LOGE(TAG, "read error");
    this->busy_ = false;
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

  ESP_LOGV(TAG, "Received buffer \"%s\" for command type %s", &buf[1],
           LOG_STR_ARG(EzoCommandTypeStrings::get_log_str(to_run->command_type, EzoCommandTypeStrings::LAST_INDEX)));

  if (buf[0] == 1) {
    std::string payload = reinterpret_cast<char *>(&buf[1]);
    if (!payload.empty()) {
      auto start_location = payload.find(',');
      switch (to_run->command_type) {
        case EzoCommandType::EZO_READ: {
          // some sensors return multiple comma-separated values, terminate string after first one
          if (start_location != std::string::npos) {
            payload.erase(start_location);
          }
          auto val = parse_number<float>(payload);
          if (!val.has_value()) {
            ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
          } else {
            this->publish_state(*val);
          }
          break;
        }
        case EzoCommandType::EZO_LED:
          this->led_callback_.call(payload.back() == '1');
          break;
        case EzoCommandType::EZO_DEVICE_INFORMATION:
          if (start_location != std::string::npos) {
            this->device_infomation_callback_.call(payload.substr(start_location + 1));
          }
          break;
        case EzoCommandType::EZO_SLOPE:
          if (start_location != std::string::npos) {
            this->slope_callback_.call(payload.substr(start_location + 1));
          }
          break;
        case EzoCommandType::EZO_CALIBRATION:
          if (start_location != std::string::npos) {
            this->calibration_callback_.call(payload.substr(start_location + 1));
          }
          break;
        case EzoCommandType::EZO_T:
          if (start_location != std::string::npos) {
            this->t_callback_.call(payload.substr(start_location + 1));
          }
          break;
        case EzoCommandType::EZO_CUSTOM:
          this->custom_callback_.call(payload);
          break;
        default:
          break;
      }
    }
  }
  this->busy_ = false;
}

void EZOSensor::add_command_(const char *command, EzoCommandType command_type, uint16_t delay_ms) {
  const size_t length = strlen(command);
  if (length > EZO_MAX_COMMAND_LENGTH) {
    ESP_LOGW(TAG, "Command too long (%u > %u), dropped", static_cast<unsigned>(length),
             static_cast<unsigned>(EZO_MAX_COMMAND_LENGTH));
    return;
  }
  if (this->queue_count_ == EZO_COMMAND_QUEUE_SIZE) {
    ESP_LOGW(TAG, "Command queue full, dropped \"%s\"", command);
    return;
  }
  EzoCommand &slot = this->queue_[(this->queue_head_ + this->queue_count_) % EZO_COMMAND_QUEUE_SIZE];
  memcpy(slot.command, command, length + 1);
  slot.delay_ms = delay_ms;
  slot.command_type = command_type;
  this->queue_count_++;
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
    this->new_address_ = address;
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
