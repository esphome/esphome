#include "ezo.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ezo {

static const char *const EZO_COMMAND_TYPE_STRINGS[] = {"EZO_READ",  "EZO_LED",         "EZO_DEVICE_INFORMATION",
                                                       "EZO_SLOPE", "EZO_CALIBRATION", "EZO_SLEEP",
                                                       "EZO_I2C",   "EZO_T",           "EZO_CUSTOM"};

static const char *const EZO_CALIBRATION_TYPE_STRINGS[] = {"LOW", "MID", "HIGH"};

void EZOSensor::dump_config() {
  LOG_SENSOR("", "EZO", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with EZO circuit failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void EZOSensor::update() {
  // Check if a read is in there already and if not insert on in the second position

  if (!this->commands_.empty() && this->commands_.front()->command_type != EzoCommandType::EZO_READ &&
      this->commands_.size() > 1) {
    bool found = false;

    for (auto &i : this->commands_) {
      if (i->command_type == EzoCommandType::EZO_READ) {
        found = true;
        break;
      }
    }

    if (!found) {
      std::unique_ptr<EzoCommand> ezo_command(new EzoCommand);
      ezo_command->command = "R";
      ezo_command->command_type = EzoCommandType::EZO_READ;
      ezo_command->delay_ms = 900;

      auto it = this->commands_.begin();
      ++it;
      this->commands_.insert(it, std::move(ezo_command));
    }

    return;
  }

  this->get_state();
}

void EZOSensor::loop() {
  if (this->commands_.empty()) {
    return;
  }

  EzoCommand *to_run = this->commands_.front().get();

  if (!to_run->command_sent) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(to_run->command.c_str());
    ESP_LOGVV(TAG, "Sending command \"%s\"", data);

    this->write(data, to_run->command.length());

    if (to_run->command_type == EzoCommandType::EZO_SLEEP ||
        to_run->command_type == EzoCommandType::EZO_I2C) {  // Commands with no return data
      this->commands_.pop_front();
      if (to_run->command_type == EzoCommandType::EZO_I2C)
        this->address_ = this->new_address_;
      return;
    }

    this->start_time_ = millis();
    to_run->command_sent = true;
    return;
  }

  if (millis() - this->start_time_ < to_run->delay_ms)
    return;

  uint8_t buf[32];

  buf[0] = 0;

  if (!this->read_bytes_raw(buf, 32)) {
    ESP_LOGE(TAG, "read error");
    this->commands_.pop_front();
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

  ESP_LOGV(TAG, "Received buffer \"%s\" for command type %s", &buf[1], EZO_COMMAND_TYPE_STRINGS[to_run->command_type]);

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
  this->commands_.pop_front();
}

void EZOSensor::add_command_(const std::string &command, EzoCommandType command_type, uint16_t delay_ms) {
  std::unique_ptr<EzoCommand> ezo_command(new EzoCommand);
  ezo_command->command = command;
  ezo_command->command_type = command_type;
  ezo_command->delay_ms = delay_ms;
  this->commands_.push_back(std::move(ezo_command));
}

void EZOSensor::set_calibration_point_(EzoCalibrationType type, float value) {
  std::string payload = str_sprintf("Cal,%s,%0.2f", EZO_CALIBRATION_TYPE_STRINGS[type], value);
  this->add_command_(payload, EzoCommandType::EZO_CALIBRATION, 900);
}

void EZOSensor::set_address(uint8_t address) {
  if (address > 0 && address < 128) {
    std::string payload = str_sprintf("I2C,%u", address);
    this->new_address_ = address;
    this->add_command_(payload, EzoCommandType::EZO_I2C);
  } else {
    ESP_LOGE(TAG, "Invalid I2C address");
  }
}

void EZOSensor::get_device_information() { this->add_command_("i", EzoCommandType::EZO_DEVICE_INFORMATION); }

void EZOSensor::set_sleep() { this->add_command_("Sleep", EzoCommandType::EZO_SLEEP); }

void EZOSensor::get_state() { this->add_command_("R", EzoCommandType::EZO_READ, 900); }

void EZOSensor::get_slope() { this->add_command_("Slope,?", EzoCommandType::EZO_SLOPE); }

void EZOSensor::get_t() { this->add_command_("T,?", EzoCommandType::EZO_T); }

void EZOSensor::set_t(float value) {
  std::string payload = str_sprintf("T,%0.2f", value);
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
  std::string payload = str_sprintf("Cal,%0.2f", value);
  this->add_command_(payload, EzoCommandType::EZO_CALIBRATION, 900);
}

void EZOSensor::clear_calibration() { this->add_command_("Cal,clear", EzoCommandType::EZO_CALIBRATION); }

void EZOSensor::get_led_state() { this->add_command_("L,?", EzoCommandType::EZO_LED); }

void EZOSensor::set_led_state(bool on) {
  std::string to_send = "L,";
  to_send += on ? "1" : "0";
  this->add_command_(to_send, EzoCommandType::EZO_LED);
}

void EZOSensor::send_custom(const std::string &to_send) { this->add_command_(to_send, EzoCommandType::EZO_CUSTOM); }

}  // namespace ezo
}  // namespace esphome
