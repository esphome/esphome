#include "ms8607.h"
#include "esphome/core/helpers.h"
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
/// Read the humidity sensor user register
static const uint8_t MS8607_CMD_H_READ_USER_REGISTER = 0xE7;
/// Write to the humidity sensor user register
static const uint8_t MS8607_CMD_H_WRITE_USER_REGISTER = 0xE6;

static const uint8_t MS8607_CMD_ADC_READ = 0x00;

static const uint8_t MS8607_CMD_CONV_D1 = 0x40;
static const uint8_t MS8607_CMD_CONV_D2 = 0x50;

enum class MS8607Component::ErrorCode {
  /// Component hasn't failed (yet?)
  NONE = 0,
  /// Asking the Pressure/Temperature sensor to reset failed
  PT_RESET_FAILED,
  /// Asking the Humidity sensor to reset failed
  H_RESET_FAILED,
  /// Reading the PROM calibration values failed
  PROM_READ_FAILED,
  /// The PROM calibration values failed the CRC check
  PROM_CRC_FAILED,
};

// TODO: these values are from data sheet. Sparkfun/Adafruit libraries have different values??
enum class MS8607Component::HumidityResolution {
  OSR_12B = 0x00,
  OSR_11B = 0x01,
  OSR_10B = 0x80,
  OSR_8b = 0x81,
};
/// Mask for bits of the Humidity Resolution, in the humidity sensor's user register.
static const uint8_t MS8607_HUMIDITY_RESOLUTION_MASK = 0x81;

static uint8_t crc4(uint16_t *buffer, size_t length);

void MS8607Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MS8607...");
  this->error_code_ = ErrorCode::NONE;

  ESP_LOGD(TAG, "Resetting both I2C addresses: 0x%02X, 0x%02X",
           this->address_, this->humidity_sensor_address_);
  // I believe sending the reset command to both addresses is preferable to
  // skipping humidity if PT fails for some reason.
  // However, only consider the reset successful if they both ACK
  bool pt_successful = this->write_bytes(MS8607_PT_CMD_RESET, nullptr, 0);
  bool h_successful = this->humidity_i2c_device_->write_bytes(MS8607_CMD_H_RESET, nullptr, 0);

  if (pt_successful && h_successful) {
    // TODO: blocking wait? Or use set_timeout? I think 15ms is short enough to just block?
    delay(15); // matches Adafruit_MS8607 & SparkFun_PHT_MS8607_Arduino_Library
  } else {
    ESP_LOGE(TAG, "Resetting I2C devices failed. Marking component as failed.");
    if (h_successful) {
      this->error_code_ = ErrorCode::PT_RESET_FAILED;
    } else {
      this->error_code_ = ErrorCode::H_RESET_FAILED;
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
  auto f = std::bind(&MS8607Component::read_humidity, this, OSR_8b);
  this->set_timeout("OSR_8b", 10, f);

  auto f2 = std::bind(&MS8607Component::read_humidity_, this, OSR_10B);
  this->set_timeout("OSR_10B", 1000, f2);

  auto f3 = std::bind(&MS8607Component::read_humidity_, this, OSR_11B);
  this->set_timeout("OSR_11B", 2000, f3);
  
  auto f4 = std::bind(&MS8607Component::read_humidity_, this, OSR_12B);
  this->set_timeout("OSR_12B", 3000, f4);
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
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->humidity_sensor_address_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MS8607 failed! Reason: %u",
             static_cast<uint8_t>(this->error_code_));
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

  uint16_t buffer[MS8607_PROM_COUNT];
  bool successful = true;

  for (uint8_t idx = 0; idx < MS8607_PROM_COUNT; ++idx) {
    uint8_t address_to_read = MS8607_PROM_START + (idx * 2);
    successful &= this->read_byte_16(address_to_read, &buffer[idx]);
  }

  if (!successful) {
    ESP_LOGE(TAG, "Reading calibration values from PROM failed");
    this->error_code_ = ErrorCode::PROM_READ_FAILED;
    return false;
  }

  ESP_LOGD(TAG, "Checking CRC of calibration values from PROM");
  uint8_t expected_crc = (buffer[0] & 0xF000) >> 12; // first 4 bits
  buffer[0] &= 0x0FFF; // strip CRC from buffer, in order to run CRC
  uint8_t actual_crc = crc4(buffer, MS8607_PROM_COUNT);

  if (expected_crc != actual_crc) {
    ESP_LOGE(TAG, "Incorrect CRC value. Provided value 0x%01X != calculated value 0x%01X",
             expected_crc, actual_crc);
    this->error_code_ = ErrorCode::PROM_CRC_FAILED;
    return false;
  }

  this->calibration_values_.pressure_sensitivity = buffer[1];
  this->calibration_values_.pressure_offset = buffer[2];
  this->calibration_values_.pressure_sensitivity_temperature_coefficient = buffer[3];
  this->calibration_values_.pressure_offset_temperature_coefficient = buffer[4];
  this->calibration_values_.reference_temperature = buffer[5];
  this->calibration_values_.temperature_coefficient_of_temperature = buffer[6];
  ESP_LOGD(TAG, "Finished reading calibration values");

  return true;
}

/**
 CRC-4 algorithm from datasheet. It operates on a buffer of 16-bit values, one byte at a time, using a 16-bit
 value to collect the CRC result into.

 The provided/expected CRC value must already be zeroed out from the buffer.
 */
static uint8_t crc4(uint16_t *buffer, size_t length) {
  uint16_t crc_remainder = 0;

  // algorithm to add a byte into the crc
  auto apply_crc = [&crc_remainder](uint8_t next) {
    crc_remainder ^= next;
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (crc_remainder & 0x8000) {
        crc_remainder = (crc_remainder << 1) ^ 0x3000;
      } else {
        crc_remainder = (crc_remainder << 1);
      }
    }
  };

  // add all the bytes
  for (uint8_t idx = 0; idx < length; ++idx) {
    for (auto byte : decode_uint16(buffer[idx])) {
      apply_crc(byte);
    }
  }
  // For the MS8607 CRC, add a pair of zeros to shift the last byte from `buffer` through
  apply_crc(0);
  apply_crc(0);

  return crc_remainder >> 12; // only the most significant 4 bits
}

bool MS8607Component::set_humidity_resolution_(HumidityResolution resolution) {
  ESP_LOGD(TAG, "Setting humidity sensor resolution to 0x%02X", static_cast<uint8_t>(resolution));
  uint8_t register_value;

  if (!this->humidity_i2c_device_->read_byte(MS8607_CMD_H_READ_USER_REGISTER, &register_value)) {
    ESP_LOGE(TAG, "Setting humidity sensor resolution failed while reading user register");
    return false;
  }

  // clear previous resolution & then set new resolution
  register_value = ((register_value & ~MS8607_HUMIDITY_RESOLUTION_MASK)
                    | (resolution & MS8607_HUMIDITY_RESOLUTION_MASK));

  if (!this->humidity_i2c_device_->write_byte(MS8607_CMD_H_WRITE_USER_REGISTER, register_value)) {
    ESP_LOGE(TAG, "Setting humidity sensor resolution failed while writing user register");
    return false;
  }

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

void MS8607Component::read_humidity_(HumidityResolution resolution) {
  this->set_humidity_resolution_(resolution);

  uint8_t bytes[3];
  uint8_t failure_count = 0;
  // FIXME: instead of blocking wait, use non-blocking + set_interval
  while (!this->humidity_i2c_device_->read_bytes(0xE5, &bytes, 3, 50)) {
    ESP_LOGD(TAG, "Humidity not ready");
    if (++failure_count > 5) {
      return;
    }
    delay(25);
  }

  uint16_t humidity = encode_uint16(buffer[0], buffer[1]);
  if (!(humidity & 0x2)) {
    ESP_LOGE(TAG, "Status bit in humidity data was not set?");
  }
  humidity &= ~(0b11); // strip status & unassigned bits from data

  ESP_LOGD(TAG, "Read humidity binary value 0x%04X", humidity);

  // map 16 bit humidity value into range [-6%, 118%]
  float humidity_percentage = lerp(humidity / (1 << 16), -6.0, 118.0);
  ESP_LOGD(TAG, "Read humidity percentage of %.4f", humidity_percentage);

  // TODO: compensate for temperature
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
