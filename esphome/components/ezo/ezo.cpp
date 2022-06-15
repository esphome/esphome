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

void EZOSensor::send_command(const std::string &payload, EzoCommandType type, uint16_t delay_ms = 300,
                             bool response_expected = true) {
  if (this->cmd_response_expected && !this->cmd_completed) {
    ESP_LOGE(TAG, "send_command skipped, still waiting for previous response.");
    return;
  }

  this->cmd_sent = false;
  this->cmd_completed = false;
  this->cmd_payload = &payload;
  this->cmd_type = type;
  this->cmd_delay_ms = delay_ms;
  this->cmd_response_expected = response_expected;
}

void EZOSensor::update() { this->get_state(); }

void EZOSensor::loop() {
  // In case the current command is completed, we do nothing
  if (this->cmd_completed)
    return;

  // In case the current command is not sent, we send it
  if (!this->cmd_sent) {
    const auto *data = reinterpret_cast<const uint8_t *>(&this->cmd_payload.c_str()[0]);
    ESP_LOGVV(TAG, "Sending command \"%s\"", data);

    this->write(data, this->cmd_payload.length());
    this->start_time_ = millis();
    this->next_command_after_ = this->start_time_ + this->cmd_delay_ms;
    this->cmd_sent = true;
    return;
  }

  // We wait at least the delay time
  if (millis() < this->next_command_after_)
    return;

  // But in case no response is expected, we try to get the new state
  if (!this->cmd_response_expected) {
    this->get_state();
    return;
  }

  uint8_t buf[32];
  buf[0] = 0;

  if (!this->read_bytes_raw(buf, 32)) {
    ESP_LOGE(TAG, "Read error!");
    this->cmd_completed = true;
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

  ESP_LOGVV(TAG, "Received buffer \"%s\" for command type %s", buf, EZO_COMMAND_TYPE_STRINGS[this->cmd_type]);

  // for (int index = 0; index < 32; ++index) {
  //   ESP_LOGD(TAG, "Received buffer index: %d char: \"%c\" %d", index, buf[index], buf[index]);
  // }

  if (buf[0] == 1 || this->cmd_type == EzoCommandType::EZO_CALIBRATION) {  // EZO_CALIBRATION returns 0-3
    std::string payload = reinterpret_cast<char *>(&buf[1]);
    if (!payload.empty()) {
      switch (this->cmd_type) {
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
      this->cmd_completed = true;
    }
  }
}
}  // namespace ezo
}  // namespace esphome
