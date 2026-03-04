#include "hdc302x.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::hdc302x {

static const char *const TAG = "hdc302x.sensor";

// Commands (per datasheet Table 7-4)
static const uint8_t HDC302X_CMD_SOFT_RESET[2] = {0x30, 0xa2};
static const uint8_t HDC302X_CMD_CLEAR_STATUS_REGISTER[2] = {0x30, 0x41};

static const uint8_t HDC302X_CMD_TRIGGER_MSB = 0x24;

static const uint8_t HDC302X_CMD_HEATER_ENABLE[2] = {0x30, 0x6d};
static const uint8_t HDC302X_CMD_HEATER_DISABLE[2] = {0x30, 0x66};
static const uint8_t HDC302X_CMD_HEATER_CONFIGURE[2] = {0x30, 0x6e};

void HDC302XComponent::setup() {
  // Soft reset the device
  if (this->write(HDC302X_CMD_SOFT_RESET, 2) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Soft reset failed"));
    return;
  }
  // Delay SensorRR (reset ready), per datasheet, 6.5.
  delay(3);

  // Clear status register
  if (this->write(HDC302X_CMD_CLEAR_STATUS_REGISTER, 2) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Clear status failed"));
    return;
  }
}

void HDC302XComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HDC302x:\n"
                "  Heater: %s",
                this->heater_active_ ? "active" : "inactive");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temp_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void HDC302XComponent::update() {
  uint8_t cmd[] = {
      HDC302X_CMD_TRIGGER_MSB,
      this->power_mode_,
  };
  if (this->write(cmd, 2) != i2c::ERROR_OK) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Read data after ADC conversion has completed
  this->set_timeout(this->conversion_delay_ms_(), [this]() { this->read_data_(); });
}

void HDC302XComponent::start_heater(uint16_t power, uint32_t duration_ms) {
  if (!this->disable_heater_()) {
    ESP_LOGD(TAG, "Heater disable before start failed");
  }
  if (!this->configure_heater_(power) || !this->enable_heater_()) {
    ESP_LOGW(TAG, "Heater start failed");
    return;
  }
  this->heater_active_ = true;
  this->cancel_timeout("heater_off");
  if (duration_ms > 0) {
    this->set_timeout("heater_off", duration_ms, [this]() { this->stop_heater(); });
  }
}

void HDC302XComponent::stop_heater() {
  this->cancel_timeout("heater_off");
  if (!this->disable_heater_()) {
    ESP_LOGW(TAG, "Heater stop failed");
  }
  this->heater_active_ = false;
}

bool HDC302XComponent::enable_heater_() {
  if (this->write(HDC302X_CMD_HEATER_ENABLE, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Enable heater failed");
    return false;
  }
  return true;
}

bool HDC302XComponent::configure_heater_(uint16_t power_level) {
  if (power_level > 0x3fff) {
    ESP_LOGW(TAG, "Heater power 0x%04x exceeds max 0x3fff", power_level);
    return false;
  }

  // Heater current level config.
  uint8_t config[] = {
      static_cast<uint8_t>((power_level >> 8) & 0xff),  // MSB
      static_cast<uint8_t>(power_level & 0xff)          // LSB
  };

  // Configure level of heater current (per datasheet 7.5.7.8).
  uint8_t cmd[] = {
      HDC302X_CMD_HEATER_CONFIGURE[0],   HDC302X_CMD_HEATER_CONFIGURE[1], config[0], config[1],
      crc8(config, 2, 0xff, 0x31, true),
  };
  if (this->write(cmd, sizeof(cmd)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Configure heater failed");
    return false;
  }

  return true;
}

bool HDC302XComponent::disable_heater_() {
  if (this->write(HDC302X_CMD_HEATER_DISABLE, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Disable heater failed");
    return false;
  }
  return true;
}

void HDC302XComponent::read_data_() {
  uint8_t buf[6];
  if (this->read(buf, 6) != i2c::ERROR_OK) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Check checksums
  if (crc8(buf, 2, 0xff, 0x31, true) != buf[2] || crc8(buf + 3, 2, 0xff, 0x31, true) != buf[5]) {
    this->status_set_warning(LOG_STR("Read data: invalid CRC"));
    return;
  }

  this->status_clear_warning();

  if (this->temp_sensor_ != nullptr) {
    uint16_t raw_t = encode_uint16(buf[0], buf[1]);
    // Calculate temperature in Celsius per datasheet section 7.3.3.
    float temp = -45 + 175 * (float(raw_t) / 65535.0f);
    this->temp_sensor_->publish_state(temp);
  }

  if (this->humidity_sensor_ != nullptr) {
    uint16_t raw_rh = encode_uint16(buf[3], buf[4]);
    // Calculate RH% per datasheet section 7.3.3.
    float humidity = 100 * (float(raw_rh) / 65535.0f);
    this->humidity_sensor_->publish_state(humidity);
  }
}

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
}

}  // namespace esphome::hdc302x
