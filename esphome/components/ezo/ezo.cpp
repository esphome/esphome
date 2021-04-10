#include "ezo.h"
#include "esphome/core/log.h"

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
  if (!this->commands_.empty()) {  // Maybe check if a read is in there already and if not insert in second position?
    ESP_LOGE(TAG, "update overrun, still waiting for previous response");  // Not sure if we care to log
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
    auto data = reinterpret_cast<const uint8_t *>(&to_run->command.c_str()[0]);
    ESP_LOGD(TAG, "Sending command \"%s\"", data);

    // for (uint8_t i = 0; i < to_run->command.length(); i++) {
    //   ESP_LOGD(TAG, "Sending index: %d char: \"%c\" hex: 0x%02X", i, data[i], data[i]);
    // }

    this->write_bytes_raw(data, to_run->command.length());

    if (to_run->command_type == EzoCommandType::EZO_SLEEP ||
        to_run->command_type == EzoCommandType::EZO_I2C) {  // Commands with no return data
      delete to_run;
      this->commands_.pop_front();
      return;
    }

    this->start_time_ = millis();
    to_run->command_sent = true;
    return;
  }

  if (millis() - this->start_time_ < to_run->delay_ms)
    return;

  uint8_t buf[20];

  buf[0] = 0;

  if (!this->read_bytes_raw(buf, 20)) {
    ESP_LOGE(TAG, "read error");
    delete to_run;
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

  ESP_LOGD(TAG, "Received buffer \"%s\" for command type %s", buf, EzoCommandTypeStrings[to_run->command_type]);
  if (buf[0] == 1) {
    std::string payload = reinterpret_cast<char *>(&buf[1]);
    if (!payload.empty()) {
      switch (to_run->command_type) {
        case EzoCommandType::EZO_READ: {
          auto val = parse_float(payload);
          if (!val.has_value()) {
            ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
          } else {
            this->publish_state(*val);
          }

          break;
        }
        case EzoCommandType::EZO_LED: {
          break;
        }
        case EzoCommandType::EZO_DEVICE_INFORMATION: {
          break;
        }
        case EzoCommandType::EZO_SLOPE: {
          break;
        }
        case EzoCommandType::EZO_CALIBRATION: {
          break;
        }
        case EzoCommandType::EZO_T: {
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  delete to_run;
  this->commands_.pop_front();
}

// T
void EZOSensor::set_t(std::string value) {
  std::string to_send = "T," + value;
  this->add_command(to_send, EzoCommandType::EZO_T);
}

// Calibration
void EZOSensor::set_calibration(std::string point, std::string value) {
  std::string to_send = "Cal," + point + "," + value;
  this->add_command(to_send, EzoCommandType::EZO_CALIBRATION);
}

// LED control
void EZOSensor::set_led_state(bool on) {
  std::string to_send = "L,";
  to_send += on ? "1" : "0";
  this->add_command(to_send, EzoCommandType::EZO_LED);
}

}  // namespace ezo
}  // namespace esphome
