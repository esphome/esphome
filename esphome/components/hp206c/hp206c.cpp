#include "hp206c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hp206c {

static const char *const TAG = "hp206c.sensor";

static const uint8_t HP206C_ADDRESS = 0x76;
static const uint8_t HP206C_COMMAND_SOFT_RST = 0x06;
static const uint8_t HP206C_COMMAND_READ_PT = 0x10;
static const uint8_t HP206C_COMMAND_READ_AT = 0x11;
static const uint8_t HP206C_COMMAND_READ_P = 0x30;
static const uint8_t HP206C_COMMAND_READ_A = 0x31;
static const uint8_t HP206C_COMMAND_READ_T = 0x32;
static const uint8_t HP206C_COMMAND_ANA_CAL = 0x28;
static const uint8_t HP206C_CHANNEL_TEMP_PRESSURE = 0b00;
static const uint8_t HP206C_CHANNEL_TEMP_ONLY = 0b10;

static constexpr uint8_t hp206c_command_adc_cvt(uint8_t osr, uint8_t channel) {
  return (0x40 + ((osr & 0x0F) << 2) + (channel & 0x03));
}

void HP206CComponent::update() {
  uint8_t mode;
  if (this->pressure_ != nullptr) {
    mode = hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_2048, HP206C_CHANNEL_TEMP_PRESSURE);
  } else if (this->pressure_ == nullptr && this->temperature_ != nullptr) {
    mode = hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_2048, HP206C_CHANNEL_TEMP_ONLY);
  } else {
    return;
  }

  if (!this->start_conversion_(mode))
    return;

  this->set_timeout("hp206c_measure", conversion_time_(mode), [this]() { this->read_data_(); });
}

void HP206CComponent::setup() { ESP_LOGCONFIG(TAG, "Setting up HP206C..."); }

void HP206CComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HP206C:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with HP206C failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  OSR setting 0x%02X ", this->filter_mode_);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Pressure", this->pressure_);
}

float HP206CComponent::get_setup_priority() const { return setup_priority::DATA; }

bool HP206CComponent::start_conversion_(uint8_t mode) {
  ESP_LOGV(TAG, "Starting conversion 0x%02X...", mode);
  return this->write_bytes(mode, nullptr, 0);
}

void HP206CComponent::read_data_() {
  uint8_t buffer[6];

  if (this->pressure_ != nullptr && this->temperature_ != nullptr) {
    if (!this->write_bytes(HP206C_COMMAND_READ_PT, nullptr, 0)) {
      this->status_set_warning();
      return;
    }
    if (!this->read_bytes_raw(buffer, 6)) {
      this->status_set_warning();
      return;
    }
    const float pressure = (((buffer[3] & 0x0F) << 16) + (buffer[4] << 8) + buffer[5]) / 100.0;
    this->pressure_->publish_state(pressure);
    float temperature;
    if (buffer[0] & 0x08) {
      temperature = ((((buffer[0] & 0x0F) << 16) + (buffer[1] << 8) + buffer[2]) - (1 << 20)) / 100.0;
    } else {
      temperature = (((buffer[0] & 0x0F) << 16) + (buffer[1] << 8) + buffer[2]) / 100.0;
    }
    this->temperature_->publish_state(temperature);
  } else if (this->pressure_ == nullptr && this->temperature_ != nullptr) {
    if (!this->write_bytes(HP206C_COMMAND_READ_T, nullptr, 0)) {
      this->status_set_warning();
      return;
    }
    if (!this->read_bytes_raw(buffer, 3)) {
      this->status_set_warning();
      return;
    }
    float temperature;
    if (buffer[0] & 0x08) {
      temperature = ((((buffer[0] & 0x0F) << 16) + (buffer[1] << 8) + buffer[2]) - (1 << 20)) / 100.0;
    } else {
      temperature = (((buffer[0] & 0x0F) << 16) + (buffer[1] << 8) + buffer[2]) / 100.0;
    }
    this->temperature_->publish_state(temperature);
  } else if (this->pressure_ != nullptr && this->temperature_ == nullptr) {
    if (!this->write_bytes(HP206C_COMMAND_READ_P, nullptr, 0)) {
      this->status_set_warning();
      return;
    }
    if (!this->read_bytes_raw(buffer, 3)) {
      this->status_set_warning();
      return;
    }
    const float pressure = (((buffer[0] & 0x0F) << 16) + (buffer[1] << 8) + buffer[2]) / 100.0;
    this->pressure_->publish_state(pressure);
  }
}

int HP206CComponent::conversion_time_(uint8_t mode) {
  switch (mode) {
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_128, HP206C_CHANNEL_TEMP_ONLY):
      return 3;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_256, HP206C_CHANNEL_TEMP_ONLY):
      return 5;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_512, HP206C_CHANNEL_TEMP_ONLY):
      return 9;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_1024, HP206C_CHANNEL_TEMP_ONLY):
      return 17;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_2048, HP206C_CHANNEL_TEMP_ONLY):
      return 33;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_4096, HP206C_CHANNEL_TEMP_ONLY):
      return 66;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_128, HP206C_CHANNEL_TEMP_PRESSURE):
      return 5;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_256, HP206C_CHANNEL_TEMP_PRESSURE):
      return 9;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_512, HP206C_CHANNEL_TEMP_PRESSURE):
      return 17;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_1024, HP206C_CHANNEL_TEMP_PRESSURE):
      return 33;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_2048, HP206C_CHANNEL_TEMP_PRESSURE):
      return 66;
    case hp206c_command_adc_cvt(HP206C_CHANNEL_OSR_4096, HP206C_CHANNEL_TEMP_PRESSURE): /* fallthrough */
    default:
      return 132;
  }
}

}  // namespace hp206c
}  // namespace esphome
