#include "ezo.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ezo {

static const char *const TAG = "ezo.sensor";

void EZOSensor::dump_config() {
  LOG_SENSOR("", "EZO", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
    ESP_LOGE(TAG, "Communication with EZO circuit failed!");
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
      std::deque<EzoCommand *>::iterator it = this->commands_.begin();
      ++it;
      this->commands_.insert(it, EzoCommand("R", EzoCommandType::EZO_READ, 900));
    }

    return;
  }

  this->get_state();
}

void EZOSensor::loop() {
  if (this->commands_.empty()) {
    return;
  }

  EzoCommand * cur_ezo_cmd = this->commands_.front();

  if (!cur_ezo_cmd->command_sent) {
    const auto *data = reinterpret_cast<const uint8_t *>(&cur_ezo_cmd->command.c_str()[0]);
    ESP_LOGVV(TAG, "Sending command \"%s\"", data);

    this->write(data, cur_ezo_cmd->command.length());
    this->start_time_ = millis();
    cur_ezo_cmd->command_sent = true;

    // Commands with no return data
    if (cur_ezo_cmd->command_type == EzoCommandType::EZO_SLEEP ||
        cur_ezo_cmd->command_type == EzoCommandType::EZO_I2C) {
      delete cur_ezo_cmd;
      this->commands_.pop_front();
      return;
    }
    return;
  }

  if (millis() - this->start_time_ < cur_ezo_cmd->delay_ms)
    return;

  uint8_t buf[32];
  buf[0] = 0;

  if (!this->read_bytes_raw(buf, 32)) {
    ESP_LOGE(TAG, "read error");
    delete cur_ezo_cmd;
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

  ESP_LOGVV(TAG, "Received buffer \"%s\" for command type %s", buf,
            EZO_COMMAND_TYPE_STRINGS[cur_ezo_cmd->command_type]);

  // for (int index = 0; index < 32; ++index) {
  //   ESP_LOGD(TAG, "Received buffer index: %d char: \"%c\" %d", index, buf[index], buf[index]);
  // }

  if (buf[0] == 1 || cur_ezo_cmd->command_type == EzoCommandType::EZO_CALIBRATION) {  // EZO_CALIBRATION returns 0-3
    std::string payload = reinterpret_cast<char *>(&buf[1]);
    if (!payload.empty()) {
      switch (cur_ezo_cmd->command_type) {
        case EzoCommandType::EZO_READ: {
          auto val = parse_number<float>(payload);
          if (!val.has_value()) {
            ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
          } else {
            this->publish_state(*val);
          }
          break;
        }
        case EzoCommandType::EZO_LED: {
          this->led_callback_.call(payload.back() == '1');
          break;
        }
        case EzoCommandType::EZO_DEVICE_INFORMATION: {
          int start_location = 0;
          if ((start_location = payload.find(',')) != std::string::npos) {
            this->device_infomation_callback_.call(payload.substr(start_location + 1));
          }
          break;
        }
        case EzoCommandType::EZO_SLOPE: {
          int start_location = 0;
          if ((start_location = payload.find(',')) != std::string::npos) {
            this->slope_callback_.call(payload.substr(start_location + 1));
          }
          break;
        }
        case EzoCommandType::EZO_CALIBRATION: {
          int start_location = 0;
          if ((start_location = payload.find(',')) != std::string::npos) {
            this->calibration_callback_.call(payload.substr(start_location + 1));
          }
          break;
        }
        case EzoCommandType::EZO_T: {
          this->t_callback_.call(payload);
          break;
        }
        case EzoCommandType::EZO_CUSTOM: {
          this->custom_callback_.call(payload);
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  delete cur_ezo_cmd;
  this->commands_.pop_front();
}
}  // namespace ezo
}  // namespace esphome
