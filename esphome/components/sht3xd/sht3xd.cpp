#include "sht3xd.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sht3xd {

static const char *const TAG = "sht3xd";

// use read serial number register with clock stretching disabled as per other SHT3XD_COMMAND registers
// which provides support for SHT85 sensor
// SHT85 does not support clock stretching and uses same registers as SHT3xd with clock stretching disabled
static const uint16_t SHT3XD_COMMAND_READ_SERIAL_NUMBER = 0x3682;

static const uint16_t SHT3XD_COMMAND_READ_STATUS = 0xF32D;
static const uint16_t SHT3XD_COMMAND_CLEAR_STATUS = 0x3041;
static const uint16_t SHT3XD_COMMAND_HEATER_ENABLE = 0x306D;
static const uint16_t SHT3XD_COMMAND_HEATER_DISABLE = 0x3066;
static const uint16_t SHT3XD_COMMAND_SOFT_RESET = 0x30A2;
static const uint16_t SHT3XD_COMMAND_POLLING_H = 0x2400;
static const uint16_t SHT3XD_COMMAND_FETCH_DATA = 0xE000;

void SHT3XDComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SHT3xD...");
  uint16_t raw_serial_number[2];
  if (!this->get_register(SHT3XD_COMMAND_READ_SERIAL_NUMBER, raw_serial_number, 2)) {
    this->mark_failed();
    return;
  }
  this->serial_number_ = (uint32_t(raw_serial_number[0]) << 16) | uint32_t(raw_serial_number[1]);

  if (!this->write_command(heater_enabled_ ? SHT3XD_COMMAND_HEATER_ENABLE : SHT3XD_COMMAND_HEATER_DISABLE)) {
    this->mark_failed();
    return;
  }
}

void SHT3XDComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SHT3xD:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with SHT3xD failed!");
    return;
  }
  ESP_LOGD(TAG, "  Serial Number: 0x%08" PRIX32, this->serial_number_);
  ESP_LOGD(TAG, "  Heater Enabled: %s", this->heater_enabled_ ? "true" : "false");

  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

float SHT3XDComponent::get_setup_priority() const { return setup_priority::DATA; }

void SHT3XDComponent::update() {
  if (this->status_has_warning()) {
    ESP_LOGD(TAG, "Retrying to reconnect the sensor.");
    this->write_command(SHT3XD_COMMAND_SOFT_RESET);
  }
  if (!this->write_command(SHT3XD_COMMAND_POLLING_H)) {
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[2];
    if (!this->read_data(raw_data, 2)) {
      this->status_set_warning();
      return;
    }

    float temperature = 175.0f * float(raw_data[0]) / 65535.0f - 45.0f;
    float humidity = 100.0f * float(raw_data[1]) / 65535.0f;

    ESP_LOGD(TAG, "Got temperature=%.2f°C humidity=%.2f%%", temperature, humidity);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    this->status_clear_warning();
  });
}

}  // namespace sht3xd
}  // namespace esphome
