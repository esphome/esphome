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

static const uint8_t MS8607_CMD_CONV_D1_OSR_8K = 0x4A;
static const uint8_t MS8607_CMD_CONV_D2_OSR_8K = 0x5A;

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
  this->read_humidity_();

  this->request_read_temperature_();
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

  // Skipping reading Humidity PROM, since it doesn't have anything interesting for us

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

  return (crc_remainder >> 12) & 0xF; // only the most significant 4 bits
}

void MS8607Component::request_read_temperature_() {
  // Tell MS8607 to start ADC conversion of temperature sensor
  if (!this->write_bytes(MS8607_CMD_CONV_D2_OSR_8K, nullptr, 0)) {
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS8607Component::read_temperature_, this);
  // datasheet says 17.2ms max conversion time at OSR 8192
  this->set_timeout("temperature", 20, f);
}

void MS8607Component::read_temperature_() {
  uint8_t bytes[3]; // 24 bits
  if (!this->read_bytes(MS8607_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }

  const uint32_t raw_temperature = encode_uint32(0, bytes[0], bytes[1], bytes[2]);
  this->request_read_pressure_(raw_temperature);
}

void MS8607Component::request_read_pressure_(uint32_t raw_temperature) {
  if (!this->write_bytes(MS8607_CMD_CONV_D1_OSR_8K, nullptr, 0)) {
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS8607Component::read_pressure_, this, raw_temperature);
  // datasheet says 17.2ms max conversion time at OSR 8192
  this->set_timeout("pressure", 20, f);
}

void MS8607Component::read_pressure_(uint32_t raw_temperature) {
  uint8_t bytes[3]; // 24 bits
  if (!this->read_bytes(MS8607_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  const uint32_t raw_pressure = encode_uint32(0, bytes[0], bytes[1], bytes[2]);
  this->calculate_values_(raw_temperature, raw_pressure);
}

void MS8607Component::read_humidity_() {
  uint8_t bytes[3];
  uint8_t failure_count = 0;
  // FIXME: instead of blocking wait, use non-blocking + set_timeout
  while (!this->humidity_i2c_device_->read_bytes(0xE5, bytes, 3)) {
    ESP_LOGD(TAG, "Humidity not ready");
    if (++failure_count > 5) {
      return;
    }
    delay(25);
  }

  uint16_t humidity = encode_uint16(bytes[0], bytes[1]);
  if (!(humidity & 0x2)) {
    ESP_LOGE(TAG, "Status bit in humidity data was not set to 1?");
  }
  humidity &= ~(0b11); // strip status & unassigned bits from data

  // FIXME: detect errors with the checksum in bytes[2]?

  ESP_LOGD(TAG, "Read humidity binary value 0x%04X", humidity);

  // map 16 bit humidity value into range [-6%, 118%]
  float humidity_partial = double(humidity) / (1 << 16);
  ESP_LOGD(TAG, "Read humidity partial of %.4f", humidity_partial); // should be [0.0, 1.0]
  float humidity_percentage = lerp(humidity_partial, -6.0, 118.0);
  ESP_LOGD(TAG, "Read humidity percentage of %.4f", humidity_percentage);

  // TODO: compensate for temperature

    this->humidity_sensor_->publish_state(humidity_percentage);
}

void MS8607Component::calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure) {
  // Perform the first order pressure/temperature calculation

  // d_t: "difference between actual and reference temperature" = D2 - [C5] * 2**8
  const int32_t d_t = int32_t(raw_temperature) - (int32_t(this->calibration_values_.reference_temperature) << 8);
  // actual temperature as hundredths of degree celsius in range [-4000, 8500]
  // 2000 + d_t * [C6] / (2**23)
  int32_t temperature = 2000 + ((int64_t(d_t) * this->calibration_values_.temperature_coefficient_of_temperature) >> 23);

  // offset at actual temperature. [C2] * (2**17) + (d_t * [C4] / (2**6))
  int64_t pressure_offset = (int64_t(this->calibration_values_.pressure_offset) << 17) + ((int64_t(d_t) * this->calibration_values_.pressure_offset_temperature_coefficient) >> 6);
  // sensitivity at actual temperature
  int64_t pressure_sensitivity = (int64_t(this->calibration_values_.pressure_sensitivity) << 16) + ((int64_t(d_t) * this->calibration_values_.pressure_sensitivity_temperature_coefficient) >> 7);

  // Perform the second order compensation, for non-linearity over temperature range
  const int64_t d_t_squared = int64_t(d_t) * d_t;
  int64_t temperature_2 = 0;
  int32_t pressure_offset_2 = 0;
  int32_t pressure_sensitivity_2 = 0;
  if (temperature < 2000) {
    const int32_t low_temperature_adjustment = (temperature - 2000) * (temperature - 2000) >> 4;

    temperature_2 = (3 * d_t_squared) >> 33;
    pressure_offset_2 = 61 * low_temperature_adjustment;
    pressure_sensitivity_2 = 29 * low_temperature_adjustment;

    if (temperature < -1500) {
      const int32_t very_low_temperature_adjustment = (temperature + 1500) * (temperature + 1500);

      pressure_offset_2 += 17 * very_low_temperature_adjustment;
      pressure_sensitivity_2 += 9 * very_low_temperature_adjustment;
    }
  } else {
    temperature_2 = (5 * d_t_squared) >> 38;
  }

  temperature -= temperature_2;
  pressure_offset -= pressure_offset_2;
  pressure_sensitivity -= pressure_sensitivity_2;

  // Temperature compensated pressure. [1000, 120000] => [10.00 mbar, 1200.00 mbar]
  const int32_t pressure = (((raw_pressure * pressure_sensitivity) >> 21) - pressure_offset) >> 15;

  const float temperature_float = temperature / 100.0f;
  const float pressure_float = pressure / 100.0f;
  ESP_LOGD(TAG, "Got temperature=%0.2f°C pressure=%0.2fhPa", temperature_float, pressure_float);

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature_float);
  }
  if (this->pressure_sensor_ != nullptr) {
    this->pressure_sensor_->publish_state(pressure_float);  // hPa aka mbar
  }
  this->status_clear_warning();
}

}  // namespace ms8607
}  // namespace esphome
