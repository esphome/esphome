#include "shtcx.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace shtcx {

static const char *const TAG = "shtcx";

static const uint16_t SHTCX_COMMAND_SLEEP = 0xB098;
static const uint16_t SHTCX_COMMAND_WAKEUP = 0x3517;
static const uint16_t SHTCX_COMMAND_READ_ID_REGISTER = 0xEFC8;
static const uint16_t SHTCX_COMMAND_SOFT_RESET = 0x805D;
static const uint16_t SHTCX_COMMAND_POLLING_H = 0x7866;

inline const char *to_string(SHTCXType type) {
  switch (type) {
    case SHTCX_TYPE_SHTC3:
      return "SHTC3";
    case SHTCX_TYPE_SHTC1:
      return "SHTC1";
    default:
      return "[Unknown model]";
  }
}

void SHTCXComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SHTCx...");
  this->wake_up();
  this->soft_reset();

  if (!this->write_command(SHTCX_COMMAND_READ_ID_REGISTER)) {
    ESP_LOGE(TAG, "Error requesting Device ID");
    this->mark_failed();
    return;
  }

  uint16_t device_id_register;
  if (!this->read_data(&device_id_register, 1)) {
    ESP_LOGE(TAG, "Error reading Device ID");
    this->mark_failed();
    return;
  }

  this->sensor_id_ = device_id_register;

  if ((device_id_register & 0x3F) == 0x07) {
    if (device_id_register & 0x800) {
      this->type_ = SHTCX_TYPE_SHTC3;
    } else {
      this->type_ = SHTCX_TYPE_SHTC1;
    }
  } else {
    this->type_ = SHTCX_TYPE_UNKNOWN;
  }
  ESP_LOGCONFIG(TAG, "  Device identified: %s (%04x)", to_string(this->type_), device_id_register);
}
void SHTCXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SHTCx:");
  ESP_LOGCONFIG(TAG, "  Model: %s (%04x)", to_string(this->type_), this->sensor_id_);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SHTCx failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}
float SHTCXComponent::get_setup_priority() const { return setup_priority::DATA; }
void SHTCXComponent::update() {
  if (this->status_has_warning()) {
    ESP_LOGW(TAG, "Retrying to reconnect the sensor.");
    this->soft_reset();
  }
  if (this->type_ != SHTCX_TYPE_SHTC1) {
    this->wake_up();
  }
  if (!this->write_command(SHTCX_COMMAND_POLLING_H)) {
    ESP_LOGE(TAG, "sensor polling failed");
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(NAN);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(NAN);
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    float temperature = NAN;
    float humidity = NAN;
    uint16_t raw_data[2];
    if (!this->read_data(raw_data, 2)) {
      ESP_LOGE(TAG, "sensor read failed");
      this->status_set_warning();
    } else {
      temperature = 175.0f * float(raw_data[0]) / 65536.0f - 45.0f;
      humidity = 100.0f * float(raw_data[1]) / 65536.0f;

      ESP_LOGD(TAG, "Got temperature=%.2f°C humidity=%.2f%%", temperature, humidity);
    }
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    this->status_clear_warning();
    if (this->type_ != SHTCX_TYPE_SHTC1) {
      this->sleep();
    }
  });
}

void SHTCXComponent::soft_reset() {
  this->write_command(SHTCX_COMMAND_SOFT_RESET);
  delayMicroseconds(200);
}
void SHTCXComponent::sleep() { this->write_command(SHTCX_COMMAND_SLEEP); }

void SHTCXComponent::wake_up() {
  this->write_command(SHTCX_COMMAND_WAKEUP);
  delayMicroseconds(200);
}

}  // namespace shtcx
}  // namespace esphome
