#include "ezo.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ezo {

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
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      EzoCommand *ezo_command = new EzoCommand;
      ezo_command->command = "R";
      ezo_command->command_type = EzoCommandType::EZO_READ;
      ezo_command->delay_ms = 900;

      std::deque<EzoCommand *>::iterator it = this->commands_.begin();
      ++it;
      this->commands_.insert(it, ezo_command);
    }

    return;
  }

  this->get_state();
}

void EZOSensor::loop() {
  if (this->commands_.empty()) {
    return;
  }

  EzoCommand *to_run = this->commands_.front();

  if (!to_run->command_sent) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(to_run->command.c_str());
    ESP_LOGVV(TAG, "Sending command \"%s\"", data);

    this->write(data, to_run->command.length());

    if (to_run->command_type == EzoCommandType::EZO_SLEEP ||
        to_run->command_type == EzoCommandType::EZO_I2C) {  // Commands with no return data
      delete to_run;                                        // NOLINT(cppcoreguidelines-owning-memory)
      this->commands_.pop_front();
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
    delete to_run;  // NOLINT(cppcoreguidelines-owning-memory)
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

  ESP_LOGVV(TAG, "Received buffer \"%s\" for command type %s", buf, EZO_COMMAND_TYPE_STRINGS[to_run->command_type]);

  // for (int index = 0; index < 32; ++index) {
  //   ESP_LOGD(TAG, "Received buffer index: %d char: \"%c\" %d", index, buf[index], buf[index]);
  // }

  if ((buf[0] == 1) || (to_run->command_type == EzoCommandType::EZO_CALIBRATION)) {  // EZO_CALIBRATION returns 0-3
    // some sensors return multiple comma-separated values, terminate string after first one
    for (size_t i = 1; i < sizeof(buf) - 1; i++) {
      if (buf[i] == ',') {
        buf[i] = '\0';
        break;
      }
    }
    std::string payload = reinterpret_cast<char *>(&buf[1]);
    if (!payload.empty()) {
      switch (to_run->command_type) {
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

  delete to_run;  // NOLINT(cppcoreguidelines-owning-memory)
  this->commands_.pop_front();
}
}  // namespace ezo
}  // namespace esphome
