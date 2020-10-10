#include "ms8607.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ms8607 {

static const char *TAG = "ms8607";

/// Reset the Pressure/Temperature sensor
static const uint8_t MS8607_PT_CMD_RESET = 0x1E;

/// Beginning of PROM register addresses. Same for both i2c addresses. Each address has 16 bits of data, and
/// PROM addresses step by two, so the LSB is always 0
static const uint8_t MS8607_PROM_START = 0xA0;
/// Last PROM register address.
static const uint8_t MS8607_PROM_END = 0xAE;
/// Number of PROM registers.
static const uint8_t MS8607_PROM_COUNT = (MS8607_PROM_END - MS8607_PROM_START) >> 1;

/// Reset the Humidity sensor
static const uint8_t MS8607_CMD_H_RESET = 0xFE;

static const uint8_t MS8607_CMD_ADC_READ = 0x00;

static const uint8_t MS8607_CMD_CONV_D1 = 0x40;
static const uint8_t MS8607_CMD_CONV_D2 = 0x50;

enum class FailureReason {
  /// Component hasn't failed (yet?)
  FAILURE_REASON_NONE = 0,
  /// Asking the Pressure/Temperature sensor to reset failed
  FAILURE_REASON_PT_RESET_FAILED,
  /// Asking the Humidity sensor to reset failed
  FAILURE_REASON_H_RESET_FAILED,
  /// Reading the PROM calibration values failed
  FAILURE_REASON_PROM_READ_FAILED,
  /// The PROM calibration values failed the CRC check
  FAILURE_REASON_PROM_CRC_FAILED,
};

void MS8607Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MS8607...");
  this->failure_reason_ = FailureReason::FAILURE_REASON_NONE;

  ESP_LOGD(TAG, "Resetting both I2C addresses: 0x%02X, 0x%02X",
           this->address_, this->humidity_sensor_address_);
  // I believe sending the reset command to both addresses is preferable to
  // skipping humidity if PT fails for some reason.
  // However, only consider the reset successful if they both ACK
  bool pt_successful = this->write_bytes(MS8607_PT_CMD_RESET, nullptr, 0);
  bool h_successful = this->humidity_i2c_device_->write_bytes(MS8607_CMD_H_RESET, nullptr, 0);

  if (pt_successful && h_successful) {
    delay(15); // matches Adafruit_MS8607 & SparkFun_PHT_MS8607_Arduino_Library
  } else {
    ESP_LOGE(TAG, "Resetting I2C devices failed. Marking component as failed.");
    if (h_successful) {
      this->failure_reason_ = FailureReason::FAILURE_REASON_PT_RESET_FAILED;
    } else {
      this->failure_reason_ = FailureReason::FAILURE_REASON_H_RESET_FAILED;
    }

    this->mark_failed();
    return;
  }

  if (!this->read_calibration_values_from_prom_()) {
    this->mark_failed();
    return;
  }
}

void MS8607Component::update() {
  // TODO: implement
  return;
  // request temperature reading
  if (!this->write_bytes(MS8607_CMD_CONV_D2 + 0x08, nullptr, 0)) {
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS8607Component::read_temperature_, this);
  this->set_timeout("temperature", 10, f);
}

void MS8607Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MS8607:");
  LOG_I2C_DEVICE(this);
  // LOG_I2C_DEVICE doesn't work for humidity, the `address_` is private. Log using this object's
  // saved value for the address.
  ESP_LOGCONFIG(TAG, "  Humidity I2C Address: 0x%02X", this->humidity_sensor_address_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MS8607 failed! Reason: %d", this->failure_reason_);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void MS8607Component::set_humidity_sensor_address(uint8_t address) {
  if (this->humidity_i2c_device_) {
    delete this->humidity_i2c_device_;
  }
  this->humidity_sensor_address_ = address;
  this->humidity_i2c_device_ = new I2CDevice(this->parent_, address);
}


bool MS8607Component::read_calibration_values_from_prom_() {
  ESP_LOGD(TAG, "Reading calibration values from PROM");

  uint8_t address_to_read;
  uint16_t buffer[MS8607_PROM_COUNT];
  bool successful = true;

  for (uint8_t idx = 0; idx < MS8607_PROM_COUNT; ++idx) {
    address_to_read = MS8607_PROM_START + (idx * 2);
    successful &= this->read_byte_16(address_to_read, &buffer[idx]);
  }

  if (!successful) {
    ESP_LOGE(TAG, "Reading calibration values from PROM failed");
    this->failure_reason_ = FailureReason::FAILURE_REASON_PROM_READ_FAILED;
    return false;
  }

  // TODO: check CRC & pull out specific values
  return true;
}

void MS8607Component::read_temperature_() {
  uint8_t bytes[3];
  if (!this->read_bytes(MS8607_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  const uint32_t raw_temperature = (uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]));

  // request pressure reading
  if (!this->write_bytes(MS8607_CMD_CONV_D1 + 0x08, nullptr, 0)) {
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS8607Component::read_pressure_, this, raw_temperature);
  this->set_timeout("pressure", 10, f);
}
void MS8607Component::read_pressure_(uint32_t raw_temperature) {
  uint8_t bytes[3];
  if (!this->read_bytes(MS8607_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  const uint32_t raw_pressure = (uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]));
  this->calculate_values_(raw_temperature, raw_pressure);
}
void MS8607Component::calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure) {
  const int32_t d_t = int32_t(raw_temperature) - (uint32_t(this->prom_[4]) << 8);
  float temperature = (2000 + (int64_t(d_t) * this->prom_[5]) / 8388608.0f) / 100.0f;

  float pressure_offset = (uint32_t(this->prom_[1]) << 16) + ((this->prom_[3] * d_t) >> 7);
  float pressure_sensitivity = (uint32_t(this->prom_[0]) << 15) + ((this->prom_[2] * d_t) >> 8);

  if (temperature < 20.0f) {
    const float t2 = (d_t * d_t) / 2147483648.0f;
    const float temp20 = (temperature - 20.0f) * 100.0f;
    float pressure_offset_2 = 2.5f * temp20 * temp20;
    float pressure_sensitivity_2 = 1.25f * temp20 * temp20;
    if (temp20 < -15.0f) {
      const float temp15 = (temperature + 15.0f) * 100.0f;
      pressure_offset_2 += 7.0f * temp15;
      pressure_sensitivity_2 += 5.5f * temp15;
    }
    temperature -= t2;
    pressure_offset -= pressure_offset_2;
    pressure_sensitivity -= pressure_sensitivity_2;
  }

  const float pressure = ((raw_pressure * pressure_sensitivity) / 2097152.0f - pressure_offset) / 3276800.0f;

  ESP_LOGD(TAG, "Got temperature=%0.02f°C pressure=%0.01fhPa", temperature, pressure);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);  // hPa
  this->status_clear_warning();
}

}  // namespace ms8607
}  // namespace esphome
