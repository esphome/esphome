#include "sts3x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sts3x {

static const char *const TAG = "sts3x";

static const uint16_t STS3X_COMMAND_READ_SERIAL_NUMBER = 0x3780;
static const uint16_t STS3X_COMMAND_READ_STATUS = 0xF32D;
static const uint16_t STS3X_COMMAND_SOFT_RESET = 0x30A2;
static const uint16_t STS3X_COMMAND_POLLING_H = 0x2400;

/// Commands for future use
static const uint16_t STS3X_COMMAND_CLEAR_STATUS = 0x3041;
static const uint16_t STS3X_COMMAND_HEATER_ENABLE = 0x306D;
static const uint16_t STS3X_COMMAND_HEATER_DISABLE = 0x3066;
static const uint16_t STS3X_COMMAND_FETCH_DATA = 0xE000;

void STS3XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STS3x...");
  if (!this->write_command(STS3X_COMMAND_READ_SERIAL_NUMBER)) {
    this->mark_failed();
    return;
  }

  uint16_t raw_serial_number[2];
  if (!this->read_data(raw_serial_number, 1)) {
    this->mark_failed();
    return;
  }
  uint32_t serial_number = (uint32_t(raw_serial_number[0]) << 16);
  ESP_LOGV(TAG, "    Serial Number: 0x%08X", serial_number);
}
void STS3XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "STS3x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ST3x failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "STS3x", this);
}
float STS3XComponent::get_setup_priority() const { return setup_priority::DATA; }
void STS3XComponent::update() {
  if (this->status_has_warning()) {
    ESP_LOGD(TAG, "Retrying to reconnect the sensor.");
    this->write_command(STS3X_COMMAND_SOFT_RESET);
  }
  if (!this->write_command(STS3X_COMMAND_POLLING_H)) {
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[1];
    if (!this->read_data(raw_data, 1)) {
      this->status_set_warning();
      return;
    }

    float temperature = 175.0f * float(raw_data[0]) / 65535.0f - 45.0f;
    ESP_LOGD(TAG, "Got temperature=%.2f°C", temperature);
    this->publish_state(temperature);
    this->status_clear_warning();
  });
}

}  // namespace sts3x
}  // namespace esphome
