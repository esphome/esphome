#include "hdc302x.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hdc302x {

static const char *const TAG = "hdc302x.sensor";

// Commands (per datasheet Table 7-4)
static const uint8_t HDC302X_CMD_SOFT_RESET[2] = {0x30, 0xa2};
static const uint8_t HDC302X_CMD_CLEAR_STATUS_REGISTER[2] = {0x30, 0x41};

static const uint8_t HDC302X_CMD_TRIGGER_MSB = 0x24;

static const uint8_t HDC302X_CMD_HEATER_ENABLE[2] = {0x30, 0x6d};
static const uint8_t HDC302X_CMD_HEATER_DISABLE[2] = {0x30, 0x66};

void HDC302XComponent::setup() {
  // Soft reset the device
  if (this->write(HDC302X_CMD_SOFT_RESET, 2) != i2c::ERROR_OK) {
    this->mark_failed("Soft reset failed");
    return;
  }

  // Clear status register
  if (this->write(HDC302X_CMD_CLEAR_STATUS_REGISTER, 2) != i2c::ERROR_OK) {
    this->mark_failed("Clear status failed");
    return;
  }

  // Heater
  if (this->heater_enabled_) {
    if (!this->enable_heater()) {
      this->mark_failed("Enable heater failed");
    }
  }
};

void HDC302XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC302x:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temp_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
};

void HDC302XComponent::update() {
  uint8_t cmd[] = {
      HDC302X_CMD_TRIGGER_MSB,
      this->power_mode_,
  };
  if (this->write(cmd, 2) != i2c::ERROR_OK) {
    this->status_set_warning("Measurement command failed");
    return;
  }

  // Read data after ADC conversion has completed
  this->set_timeout(this->conversion_delay_ms_(), [this]() { this->read_data_(); });
};

bool HDC302XComponent::enable_heater() {
  if (this->write(HDC302X_CMD_HEATER_ENABLE, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Enable heater failed");
    return false;
  }
  return true;
};

bool HDC302XComponent::disable_heater() {
  if (this->write(HDC302X_CMD_HEATER_DISABLE, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Disable heater failed");
    return false;
  }
  return true;
};

void HDC302XComponent::read_data_() {
  uint8_t buf[6];
  if (this->read(buf, 6) != i2c::ERROR_OK) {
    this->status_set_warning("Sensor read failed");
    return;
  }

  // Check checksums
  if (this->crc8_(buf, 2) != buf[2] || this->crc8_(buf + 3, 2) != buf[5]) {
    this->status_set_warning("Read data: invalid CRC");
    return;
  }

  this->status_clear_warning();

  if (this->temp_sensor_ != nullptr) {
    uint16_t raw_t = encode_uint16(buf[0], buf[1]);
    // Calculate temperature in Celcius per datasheet section 7.3.3.
    float temp = -45 + 175 * (float(raw_t) / 65535.0f);
    this->temp_sensor_->publish_state(temp);
  }

  if (this->humidity_sensor_ != nullptr) {
    uint16_t raw_rh = encode_uint16(buf[3], buf[4]);
    // Calculate RH% per datasheet section 7.3.3.
    float humidity = 100 * (float(raw_rh) / 65535.0f);
    this->humidity_sensor_->publish_state(humidity);
  }
};

uint32_t HDC302XComponent::conversion_delay_ms_() {
  // ADC conversion delay per datasheet, Table 7-5. - Trigger on Demand
  switch (this->power_mode_) {
    case HDC302XPowerMode::BALANCED:
      return 8;
    case HDC302XPowerMode::LOW_POWER:
      return 5;
    case HDC302XPowerMode::ULTRA_LOW_POWER:
      return 4;
    case HDC302XPowerMode::HIGH_ACCURACY:
    default:
      return 13;
  }
};

uint8_t HDC302XComponent::crc8_(const uint8_t *buf, size_t len) {
  // Compute 8-bit CRC with properties from datasheet, Table 7-1.
  //  - Initialisation: 0xff
  //  - Polynominal: 0x31

  uint8_t crc = 0xff;
  for (size_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (size_t j = 8; j > 0; j--) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc = crc << 1;
      }
    }
  }
  return crc;
}

}  // namespace hdc302x
}  // namespace esphome
