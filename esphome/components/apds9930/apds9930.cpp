#include "apds9930.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace apds9930 {

static const char *const TAG = "apds9930";

#define APDS9930_ERROR_CHECK(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }
#define APDS9930_WRITE_BYTE(reg, value) APDS9930_ERROR_CHECK(this->write_byte((reg) | APDS9930_CMD, value));

void APDS9930::setup() {
  ESP_LOGCONFIG(TAG, "Setting up APDS9930...");

  uint8_t id;
  // Read ID register with command byte
  if (!this->read_byte(APDS9930_ID | APDS9930_CMD, &id)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (id != APDS9930_ID_1 && id != APDS9930_ID_2) {
    this->error_code_ = WRONG_ID;
    this->mark_failed();
    return;
  }

  // Disable all features first
  APDS9930_WRITE_BYTE(APDS9930_ENABLE, 0x00);

  // Configure timing registers
  APDS9930_WRITE_BYTE(APDS9930_ATIME, this->atime_);
  APDS9930_WRITE_BYTE(APDS9930_PTIME, APDS9930_DEFAULT_PTIME);
  APDS9930_WRITE_BYTE(APDS9930_WTIME, APDS9930_DEFAULT_WTIME);

  // Configure proximity pulse count
  APDS9930_WRITE_BYTE(APDS9930_PPULSE, APDS9930_DEFAULT_PPULSE);

  // Configure proximity offset
  APDS9930_WRITE_BYTE(APDS9930_POFFSET, APDS9930_DEFAULT_POFFSET);

  // Configure config register
  APDS9930_WRITE_BYTE(APDS9930_CONFIG, APDS9930_DEFAULT_CONFIG);

  // Configure control register (LED drive, proximity gain, ambient gain, proximity diode)
  uint8_t control = 0;
  // LED drive strength (bits 6-7): 0=100mA, 1=50mA, 2=25mA, 3=12.5mA
  control |= (this->led_drive_ & 0b11) << 6;
  // Proximity gain (bits 2-3): 0=1x, 1=2x, 2=4x, 3=8x
  control |= (this->proximity_gain_ & 0b11) << 2;
  // Ambient light gain (bits 0-1): 0=1x, 1=8x, 2=16x, 3=120x
  control |= (this->ambient_gain_ & 0b11) << 0;
  // Proximity diode (bits 4-5): which LED to use
  control |= (this->proximity_diode_ & 0b11) << 4;
  APDS9930_WRITE_BYTE(APDS9930_CONTROL, control);

  // Build enable register value
  uint8_t enable = 0;
  enable |= APDS9930_PON;  // Power on
  if (this->is_ambient_enabled_()) {
    enable |= APDS9930_AEN;  // Ambient light enable
  }
  if (this->is_proximity_enabled_()) {
    enable |= APDS9930_PEN;  // Proximity enable
  }
  APDS9930_WRITE_BYTE(APDS9930_ENABLE, enable);

  ESP_LOGCONFIG(TAG, "APDS9930 setup complete");
}

bool APDS9930::is_ambient_enabled_() const {
#ifdef USE_SENSOR
  return this->ambient_light_sensor_ != nullptr;
#else
  return false;
#endif
}

bool APDS9930::is_proximity_enabled_() const {
#ifdef USE_SENSOR
  return this->proximity_sensor_ != nullptr;
#else
  return false;
#endif
}

void APDS9930::dump_config() {
  ESP_LOGCONFIG(TAG, "APDS9930:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Ambient Light", this->ambient_light_sensor_);
  LOG_SENSOR("  ", "Proximity", this->proximity_sensor_);
#endif

  ESP_LOGCONFIG(TAG, "  LED Drive: %u", this->led_drive_);
  ESP_LOGCONFIG(TAG, "  Proximity Gain: %u", this->proximity_gain_);
  ESP_LOGCONFIG(TAG, "  Ambient Light Gain: %u", this->ambient_gain_);
  ESP_LOGCONFIG(TAG, "  Proximity Diode: %u", this->proximity_diode_);

  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGE(TAG, "Communication with APDS9930 failed!");
        break;
      case WRONG_ID:
        ESP_LOGE(TAG, "APDS9930 has invalid ID (expected 0x12 or 0x39)!");
        break;
      default:
        ESP_LOGE(TAG, "Setting up APDS9930 failed!");
        break;
    }
  }
}

#define APDS9930_WARNING_CHECK(func, warning) \
  if (!(func)) { \
    ESP_LOGW(TAG, warning); \
    this->status_set_warning(); \
    return; \
  }

void APDS9930::update() {
  uint8_t status;
  APDS9930_WARNING_CHECK(this->read_byte(APDS9930_STATUS | APDS9930_CMD, &status), "Reading status register failed.");
  this->status_clear_warning();

  this->read_ambient_data_(status);
  this->read_proximity_data_(status);
}

void APDS9930::read_ambient_data_(uint8_t status) {
#ifndef USE_SENSOR
  return;
#else
  if (this->ambient_light_sensor_ == nullptr)
    return;

  // Check if ambient light data is valid (AVALID bit)
  if ((status & APDS9930_AVALID) == 0x00) {
    return;
  }

  uint8_t raw[4];
  // Read Ch0 and Ch1 data with auto-increment
  APDS9930_WARNING_CHECK(this->read_bytes(APDS9930_CH0DATAL | APDS9930_CMD_AUTO_INCREMENT, raw, 4),
                         "Reading ambient light values failed.");

  uint16_t ch0 = (uint16_t(raw[1]) << 8) | raw[0];
  uint16_t ch1 = (uint16_t(raw[3]) << 8) | raw[2];

  float lux = this->calculate_lux_(ch0, ch1);

  ESP_LOGD(TAG, "Got Ch0=%u Ch1=%u Lux=%.0f", ch0, ch1, lux);
  this->ambient_light_sensor_->publish_state(lux);
#endif
}

void APDS9930::read_proximity_data_(uint8_t status) {
#ifndef USE_SENSOR
  return;
#else
  if (this->proximity_sensor_ == nullptr)
    return;

  // Check if proximity data is valid (PVALID bit)
  if ((status & APDS9930_PVALID) == 0x00) {
    return;
  }

  uint8_t raw[2];
  // Read proximity data with auto-increment
  APDS9930_WARNING_CHECK(this->read_bytes(APDS9930_PDATAL | APDS9930_CMD_AUTO_INCREMENT, raw, 2),
                         "Reading proximity value failed.");

  uint16_t proximity = (uint16_t(raw[1]) << 8) | raw[0];

  ESP_LOGD(TAG, "Got Proximity=%u", proximity);
  this->proximity_sensor_->publish_state(proximity);
#endif
}

float APDS9930::calculate_lux_(uint16_t ch0, uint16_t ch1) {
  // Gain multiplier values: 1x, 8x, 16x, 120x
  static const uint8_t gain_values[4] = {1, 8, 16, 120};

  // Calculate integration time in milliseconds
  float alsit = 2.73f * (256.0f - this->atime_);

  // Calculate IAC (Integrated Ambient Count) - use the larger of the two formulas
  float iac1 = ch0 - APDS9930_ALS_B * ch1;
  float iac2 = APDS9930_ALS_C * ch0 - APDS9930_ALS_D * ch1;
  float iac = iac1 > iac2 ? iac1 : iac2;
  if (iac < 0)
    iac = 0;

  // Calculate lux per count
  float lpc = (APDS9930_GA * APDS9930_DF) / (alsit * gain_values[this->ambient_gain_]);

  // Calculate final lux value
  float lux = iac * lpc;

  return lux;
}

float APDS9930::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace apds9930
}  // namespace esphome
