#include "ms8607.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ms8607 {

/// TAG used for logging calls
static const char *const TAG = "ms8607";

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
/// Read relative humidity, without holding i2c master
static const uint8_t MS8607_CMD_H_MEASURE_NO_HOLD = 0xF5;
/// Temperature correction coefficient for Relative Humidity from datasheet
static const float MS8607_H_TEMP_COEFFICIENT = -0.18;

/// Read the converted analog value, either D1 (pressure) or D2 (temperature)
static const uint8_t MS8607_CMD_ADC_READ = 0x00;

// TODO: allow OSR to be turned down for speed and/or lower power consumption via configuration.
// ms8607 supports 6 different settings

/// Request conversion of analog D1 (pressure) with OSR=8192 (highest oversampling ratio). Takes maximum of 17.2ms
static const uint8_t MS8607_CMD_CONV_D1_OSR_8K = 0x4A;
/// Request conversion of analog D2 (temperature) with OSR=8192 (highest oversampling ratio). Takes maximum of 17.2ms
static const uint8_t MS8607_CMD_CONV_D2_OSR_8K = 0x5A;

enum class MS8607Component::ErrorCode {
  /// Component hasn't failed (yet?)
  NONE = 0,
  /// Both the Pressure/Temperature address and the Humidity address failed to reset
  PTH_RESET_FAILED = 1,
  /// Asking the Pressure/Temperature sensor to reset failed
  PT_RESET_FAILED = 2,
  /// Asking the Humidity sensor to reset failed
  H_RESET_FAILED = 3,
  /// Reading the PROM calibration values failed
  PROM_READ_FAILED = 4,
  /// The PROM calibration values failed the CRC check
  PROM_CRC_FAILED = 5,
};

enum class MS8607Component::SetupStatus {
  /// This component has not successfully reset the PT & H devices
  NEEDS_RESET,
  /// Reset commands succeeded, need to wait >= 15ms to read PROM
  NEEDS_PROM_READ,
  /// Successfully read PROM and ready to update sensors
  SUCCESSFUL,
};

static uint8_t crc4(uint16_t *buffer, size_t length);
static uint8_t hsensor_crc_check(uint16_t value);

void MS8607Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MS8607...");
  this->error_code_ = ErrorCode::NONE;
  this->setup_status_ = SetupStatus::NEEDS_RESET;

  // I do not know why the device sometimes NACKs the reset command, but
  // try 3 times in case it's a transitory issue on this boot
  this->set_retry(
      "reset", 5, 3,
      [this](const uint8_t remaining_setup_attempts) {
        ESP_LOGD(TAG, "Resetting both I2C addresses: 0x%02X, 0x%02X", this->address_,
                 this->humidity_device_->get_address());
        // I believe sending the reset command to both addresses is preferable to
        // skipping humidity if PT fails for some reason.
        // However, only consider the reset successful if they both ACK
        bool const pt_successful = this->write_bytes(MS8607_PT_CMD_RESET, nullptr, 0);
        bool const h_successful = this->humidity_device_->write_bytes(MS8607_CMD_H_RESET, nullptr, 0);

        if (!(pt_successful && h_successful)) {
          ESP_LOGE(TAG, "Resetting I2C devices failed");
          if (!pt_successful && !h_successful) {
            this->error_code_ = ErrorCode::PTH_RESET_FAILED;
          } else if (!pt_successful) {
            this->error_code_ = ErrorCode::PT_RESET_FAILED;
          } else {
            this->error_code_ = ErrorCode::H_RESET_FAILED;
          }

          if (remaining_setup_attempts > 0) {
            this->status_set_error();
          } else {
            this->mark_failed();
          }
          return RetryResult::RETRY;
        }

        this->setup_status_ = SetupStatus::NEEDS_PROM_READ;
        this->error_code_ = ErrorCode::NONE;
        this->status_clear_error();

        // 15ms delay matches datasheet, Adafruit_MS8607 & SparkFun_PHT_MS8607_Arduino_Library
        this->set_timeout("prom-read", 15, [this]() {
          if (this->read_calibration_values_from_prom_()) {
            this->setup_status_ = SetupStatus::SUCCESSFUL;
            this->status_clear_error();
          } else {
            this->mark_failed();
            return;
          }
        });

        return RetryResult::DONE;
      },
      5.0f);  // executes at now, +5ms, +25ms
}

void MS8607Component::update() {
  if (this->setup_status_ != SetupStatus::SUCCESSFUL) {
    // setup is still occurring, either because reset had to retry or due to the 15ms
    // delay needed between reset & reading the PROM values
    return;
  }

  // Updating happens async and sequentially.
  // Temperature, then pressure, then humidity
  this->request_read_temperature_();
}

void MS8607Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MS8607:");
  LOG_I2C_DEVICE(this);
  // LOG_I2C_DEVICE doesn't work for humidity, the `address_` is protected. Log using get_address()
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->humidity_device_->get_address());
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MS8607 failed.");
    switch (this->error_code_) {
      case ErrorCode::PT_RESET_FAILED:
        ESP_LOGE(TAG, "Temperature/Pressure RESET failed");
        break;
      case ErrorCode::H_RESET_FAILED:
        ESP_LOGE(TAG, "Humidity RESET failed");
        break;
      case ErrorCode::PTH_RESET_FAILED:
        ESP_LOGE(TAG, "Temperature/Pressure && Humidity RESET failed");
        break;
      case ErrorCode::PROM_READ_FAILED:
        ESP_LOGE(TAG, "Reading PROM failed");
        break;
      case ErrorCode::PROM_CRC_FAILED:
        ESP_LOGE(TAG, "PROM values failed CRC");
        break;
      case ErrorCode::NONE:
      default:
        ESP_LOGE(TAG, "Error reason unknown %u", static_cast<uint8_t>(this->error_code_));
        break;
    }
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

bool MS8607Component::read_calibration_values_from_prom_() {
  ESP_LOGD(TAG, "Reading calibration values from PROM");

  uint16_t buffer[MS8607_PROM_COUNT];
  bool successful = true;

  for (uint8_t idx = 0; idx < MS8607_PROM_COUNT; ++idx) {
    uint8_t const address_to_read = MS8607_PROM_START + (idx * 2);
    successful &= this->read_byte_16(address_to_read, &buffer[idx]);
  }

  if (!successful) {
    ESP_LOGE(TAG, "Reading calibration values from PROM failed");
    this->error_code_ = ErrorCode::PROM_READ_FAILED;
    return false;
  }

  ESP_LOGD(TAG, "Checking CRC of calibration values from PROM");
  uint8_t const expected_crc = (buffer[0] & 0xF000) >> 12;  // first 4 bits
  buffer[0] &= 0x0FFF;                                      // strip CRC from buffer, in order to run CRC
  uint8_t const actual_crc = crc4(buffer, MS8607_PROM_COUNT);

  if (expected_crc != actual_crc) {
    ESP_LOGE(TAG, "Incorrect CRC value. Provided value 0x%01X != calculated value 0x%01X", expected_crc, actual_crc);
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
  for (size_t idx = 0; idx < length; ++idx) {
    for (auto byte : decode_value(buffer[idx])) {
      apply_crc(byte);
    }
  }
  // For the MS8607 CRC, add a pair of zeros to shift the last byte from `buffer` through
  apply_crc(0);
  apply_crc(0);

  return (crc_remainder >> 12) & 0xF;  // only the most significant 4 bits
}

/**
 * @brief Calculates CRC value for the provided humidity (+ status bits) value
 *
 * CRC-8 check comes from other MS8607 libraries on github. I did not find it in the datasheet,
 * and it differs from the crc8 implementation that's already part of esphome.
 *
 * @param value two byte humidity sensor value read from i2c
 * @return uint8_t computed crc value
 */
static uint8_t hsensor_crc_check(uint16_t value) {
  uint32_t polynom = 0x988000;  // x^8 + x^5 + x^4 + 1
  uint32_t msb = 0x800000;
  uint32_t mask = 0xFF8000;
  uint32_t result = (uint32_t) value << 8;  // Pad with zeros as specified in spec

  while (msb != 0x80) {
    // Check if msb of current value is 1 and apply XOR mask
    if (result & msb) {
      result = ((result ^ polynom) & mask) | (result & ~mask);
    }

    // Shift by one
    msb >>= 1;
    mask >>= 1;
    polynom >>= 1;
  }
  return result & 0xFF;
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
  uint8_t bytes[3];  // 24 bits
  if (!this->read_bytes(MS8607_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }

  const uint32_t d2_raw_temperature = encode_uint32(0, bytes[0], bytes[1], bytes[2]);
  this->request_read_pressure_(d2_raw_temperature);
}

void MS8607Component::request_read_pressure_(uint32_t d2_raw_temperature) {
  if (!this->write_bytes(MS8607_CMD_CONV_D1_OSR_8K, nullptr, 0)) {
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS8607Component::read_pressure_, this, d2_raw_temperature);
  // datasheet says 17.2ms max conversion time at OSR 8192
  this->set_timeout("pressure", 20, f);
}

void MS8607Component::read_pressure_(uint32_t d2_raw_temperature) {
  uint8_t bytes[3];  // 24 bits
  if (!this->read_bytes(MS8607_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  const uint32_t d1_raw_pressure = encode_uint32(0, bytes[0], bytes[1], bytes[2]);
  this->calculate_values_(d2_raw_temperature, d1_raw_pressure);
}

void MS8607Component::request_read_humidity_(float temperature_float) {
  if (!this->humidity_device_->write_bytes(MS8607_CMD_H_MEASURE_NO_HOLD, nullptr, 0)) {
    ESP_LOGW(TAG, "Request to measure humidity failed");
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS8607Component::read_humidity_, this, temperature_float);
  // datasheet says 15.89ms max conversion time at OSR 8192
  this->set_timeout("humidity", 20, f);
}

void MS8607Component::read_humidity_(float temperature_float) {
  uint8_t bytes[3];
  if (!this->humidity_device_->read_bytes_raw(bytes, 3)) {
    ESP_LOGW(TAG, "Failed to read the measured humidity value");
    this->status_set_warning();
    return;
  }

  // "the measurement is stored into 14 bits. The two remaining LSBs are used for transmitting status information.
  // Bit1 of the two LSBS must be set to '1'. Bit0 is currently not assigned"
  uint16_t humidity = encode_uint16(bytes[0], bytes[1]);
  uint8_t const expected_crc = bytes[2];
  uint8_t const actual_crc = hsensor_crc_check(humidity);
  if (expected_crc != actual_crc) {
    ESP_LOGE(TAG, "Incorrect Humidity CRC value. Provided value 0x%01X != calculated value 0x%01X", expected_crc,
             actual_crc);
    this->status_set_warning();
    return;
  }
  if (!(humidity & 0x2)) {
    // data sheet says Bit1 should always set, but nothing about what happens if it isn't
    ESP_LOGE(TAG, "Humidity status bit was not set to 1?");
  }
  humidity &= ~(0b11);  // strip status & unassigned bits from data

  // map 16 bit humidity value into range [-6%, 118%]
  float const humidity_partial = double(humidity) / (1 << 16);
  float const humidity_percentage = lerp(humidity_partial, -6.0, 118.0);
  float const compensated_humidity_percentage =
      humidity_percentage + (20 - temperature_float) * MS8607_H_TEMP_COEFFICIENT;
  ESP_LOGD(TAG, "Compensated for temperature, humidity=%.2f%%", compensated_humidity_percentage);

  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->publish_state(compensated_humidity_percentage);
  }
  this->status_clear_warning();
}

void MS8607Component::calculate_values_(uint32_t d2_raw_temperature, uint32_t d1_raw_pressure) {
  // Perform the first order pressure/temperature calculation

  // d_t: "difference between actual and reference temperature" = D2 - [C5] * 2**8
  const int32_t d_t = int32_t(d2_raw_temperature) - (int32_t(this->calibration_values_.reference_temperature) << 8);
  // actual temperature as hundredths of degree celsius in range [-4000, 8500]
  // 2000 + d_t * [C6] / (2**23)
  int32_t temperature =
      2000 + ((int64_t(d_t) * this->calibration_values_.temperature_coefficient_of_temperature) >> 23);

  // offset at actual temperature. [C2] * (2**17) + (d_t * [C4] / (2**6))
  int64_t pressure_offset = (int64_t(this->calibration_values_.pressure_offset) << 17) +
                            ((int64_t(d_t) * this->calibration_values_.pressure_offset_temperature_coefficient) >> 6);
  // sensitivity at actual temperature. [C1] * (2**16) + ([C3] * d_t) / (2**7)
  int64_t pressure_sensitivity =
      (int64_t(this->calibration_values_.pressure_sensitivity) << 16) +
      ((int64_t(d_t) * this->calibration_values_.pressure_sensitivity_temperature_coefficient) >> 7);

  // Perform the second order compensation, for non-linearity over temperature range
  const int64_t d_t_squared = int64_t(d_t) * d_t;
  int64_t temperature_2 = 0;
  int32_t pressure_offset_2 = 0;
  int32_t pressure_sensitivity_2 = 0;
  if (temperature < 2000) {
    // (TEMP - 2000)**2 / 2**4
    const int32_t low_temperature_adjustment = (temperature - 2000) * (temperature - 2000) >> 4;

    // T2 = 3 * (d_t**2) / 2**33
    temperature_2 = (3 * d_t_squared) >> 33;
    // OFF2 = 61 * (TEMP-2000)**2 / 2**4
    pressure_offset_2 = 61 * low_temperature_adjustment;
    // SENS2 = 29 * (TEMP-2000)**2 / 2**4
    pressure_sensitivity_2 = 29 * low_temperature_adjustment;

    if (temperature < -1500) {
      // (TEMP+1500)**2
      const int32_t very_low_temperature_adjustment = (temperature + 1500) * (temperature + 1500);

      // OFF2 = OFF2 + 17 * (TEMP+1500)**2
      pressure_offset_2 += 17 * very_low_temperature_adjustment;
      // SENS2 = SENS2 + 9 * (TEMP+1500)**2
      pressure_sensitivity_2 += 9 * very_low_temperature_adjustment;
    }
  } else {
    // T2 = 5 * (d_t**2) / 2**38
    temperature_2 = (5 * d_t_squared) >> 38;
  }

  temperature -= temperature_2;
  pressure_offset -= pressure_offset_2;
  pressure_sensitivity -= pressure_sensitivity_2;

  // Temperature compensated pressure. [1000, 120000] => [10.00 mbar, 1200.00 mbar]
  const int32_t pressure = (((d1_raw_pressure * pressure_sensitivity) >> 21) - pressure_offset) >> 15;

  const float temperature_float = temperature / 100.0f;
  const float pressure_float = pressure / 100.0f;
  ESP_LOGD(TAG, "Temperature=%0.2f°C, Pressure=%0.2fhPa", temperature_float, pressure_float);

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature_float);
  }
  if (this->pressure_sensor_ != nullptr) {
    this->pressure_sensor_->publish_state(pressure_float);  // hPa aka mbar
  }
  this->status_clear_warning();

  if (this->humidity_sensor_ != nullptr) {
    // now that we have temperature (to compensate the humidity with), kick off that read
    this->request_read_humidity_(temperature_float);
  }
}

}  // namespace ms8607
}  // namespace esphome
