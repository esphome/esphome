#include "ms8607.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ms8607 {

ESPHOME_LOG_TAG(TAG, "ms8607");

static constexpr uint32_t RESET_TIMEOUT_ID = 0;
static constexpr uint32_t PROM_READ_TIMEOUT_ID = 1;
static constexpr uint32_t TEMPERATURE_TIMEOUT_ID = 2;
static constexpr uint32_t PRESSURE_TIMEOUT_ID = 3;
static constexpr uint32_t HUMIDITY_TIMEOUT_ID = 4;

/// Reset the Pressure/Temperature sensor
static constexpr uint8_t MS8607_PT_CMD_RESET = 0x1E;

/// Beginning of PROM register addresses. Same for both i2c addresses. Each address has 16 bits of data, and
/// PROM addresses step by two, so the LSB is always 0
static constexpr uint8_t MS8607_PROM_START = 0xA0;
/// Last PROM register address.
static constexpr uint8_t MS8607_PROM_END = 0xAE;
/// Number of PROM registers.
static constexpr uint8_t MS8607_PROM_COUNT = (MS8607_PROM_END - MS8607_PROM_START) >> 1;

/// Reset the Humidity sensor
static constexpr uint8_t MS8607_CMD_H_RESET = 0xFE;
/// Read relative humidity, without holding i2c master
static constexpr uint8_t MS8607_CMD_H_MEASURE_NO_HOLD = 0xF5;
/// Temperature correction coefficient for Relative Humidity from datasheet
static constexpr float MS8607_H_TEMP_COEFFICIENT = -0.18f;

/// Read the converted analog value, either D1 (pressure) or D2 (temperature)
static constexpr uint8_t MS8607_CMD_ADC_READ = 0x00;

// TODO: allow OSR to be turned down for speed and/or lower power consumption via configuration.
// ms8607 supports 6 different settings

/// Request conversion of analog D1 (pressure) with OSR=8192 (highest oversampling ratio). Takes maximum of 17.2ms
static constexpr uint8_t MS8607_CMD_CONV_D1_OSR_8K = 0x4A;
/// Request conversion of analog D2 (temperature) with OSR=8192 (highest oversampling ratio). Takes maximum of 17.2ms
static constexpr uint8_t MS8607_CMD_CONV_D2_OSR_8K = 0x5A;

static uint8_t crc4(uint16_t *buffer, size_t length);

// Sanity check Temperature & Pressure Readings, these values are way out of range

/// value below which we consider a temperature reading to be invalid
static constexpr float TEMPERATURE_LOWER_LIMIT = -50.0f;
/// value above which we consider a temperature reading to be invalid
static constexpr float TEMPERATURE_UPPER_LIMIT = 100.0f;

/// value below which we consider a pressure reading to be invalid
static constexpr float PRESSURE_LOWER_LIMIT = 0.0f;
/// value above which we consider a pressure reading to be invalid
static constexpr float PRESSURE_UPPER_LIMIT = 2000.0f;

void MS8607Component::setup() {
  // I do not know why the device sometimes NACKs the reset command, but
  // try 3 times in case it's a transitory issue on this boot
  // Backoff: executes at now, +5ms, +30ms
  this->try_reset_();
}

void MS8607Component::try_reset_() {
  // I believe sending the reset command to both addresses is preferable to
  // skipping humidity if PT fails for some reason.
  // However, only consider the reset successful if they both ACK
  bool const pt_successful = this->write_bytes(MS8607_PT_CMD_RESET, nullptr, 0);
  bool h_successful = true;
  if (this->humidity_device_ != nullptr) {
    ESP_LOGD(TAG, "Resetting I2C addresses: 0x%02X, 0x%02X", this->address_, this->humidity_device_->get_i2c_address());
    h_successful = this->humidity_device_->write_bytes(MS8607_CMD_H_RESET, nullptr, 0);
  } else {
    ESP_LOGD(TAG, "Resetting I2C address: 0x%02X", this->address_);
  }

  if (!(pt_successful && h_successful)) {
    ESP_LOGE(TAG, "Resetting I2C devices failed");
    if (--this->reset_attempts_remaining_ > 0) {
      uint32_t delay = this->reset_interval_;
      this->reset_interval_ *= 5;
      this->set_timeout(RESET_TIMEOUT_ID, delay, [this]() { this->try_reset_(); });
      this->status_set_error();
      return;
    }
    // The retries set the error flag without a reason; clear it so mark_failed() can record one
    this->status_clear_error();
    if (!pt_successful && !h_successful) {
      this->mark_failed(LOG_STR("Temperature/Pressure and Humidity reset failed"));
    } else if (!pt_successful) {
      this->mark_failed(LOG_STR("Temperature/Pressure reset failed"));
    } else {
      this->mark_failed(LOG_STR("Humidity reset failed"));
    }
    return;
  }

  this->status_clear_error();

  // 15ms delay matches datasheet, Adafruit_MS8607 & SparkFun_PHT_MS8607_Arduino_Library
  this->set_timeout(PROM_READ_TIMEOUT_ID, 15, [this]() {
    if (this->read_calibration_values_from_prom_())
      this->setup_complete_ = true;
  });
}

void MS8607Component::update() {
  if (!this->setup_complete_) {
    // setup is still occurring, either because reset had to retry or due to the 15ms
    // delay needed between reset & reading the PROM values
    return;
  }

  this->update_ok_ = true;

  // Updating happens async and sequentially.
  // Temperature, then optionally pressure, then optionally humidity
  // Temperature is used for compensation of pressure & humidity, so we'll always want it
  this->request_read_temperature_();
}

void MS8607Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MS8607:");
  LOG_I2C_DEVICE(this);
  if (this->humidity_device_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->humidity_device_->get_i2c_address());
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  // Empty at boot, since the PROM is read after a delay; useful on a device that is already running
  ESP_LOGVV(TAG,
            "  Calibration Values:\n"
            "    Pressure Sensitivity: 0x%04X\n"
            "    Pressure Offset: 0x%04X\n"
            "    Pressure Sensitivity Temperature Coefficient: 0x%04X\n"
            "    Pressure Offset Temperature Coefficient: 0x%04X\n"
            "    Reference Temperature: 0x%04X\n"
            "    Temperature Coefficient of Temperature: 0x%04X",
            this->calibration_values_.pressure_sensitivity, this->calibration_values_.pressure_offset,
            this->calibration_values_.pressure_sensitivity_temperature_coefficient,
            this->calibration_values_.pressure_offset_temperature_coefficient,
            this->calibration_values_.reference_temperature,
            this->calibration_values_.temperature_coefficient_of_temperature);
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
    this->mark_failed(LOG_STR("Reading the PROM calibration values failed"));
    return false;
  }

  ESP_LOGD(TAG, "Checking CRC of calibration values from PROM");
  uint8_t const expected_crc = (buffer[0] & 0xF000) >> 12;  // first 4 bits
  buffer[0] &= 0x0FFF;                                      // strip CRC from buffer, in order to run CRC
  uint8_t const actual_crc = crc4(buffer, MS8607_PROM_COUNT);

  if (expected_crc != actual_crc) {
    ESP_LOGE(TAG, "Incorrect CRC value. Provided value 0x%01X != calculated value 0x%01X", expected_crc, actual_crc);
    this->mark_failed(LOG_STR("PROM calibration values failed the CRC check"));
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

bool MS8607Component::read_adc_(uint32_t &value) {
  uint8_t bytes[3];  // 24 bits
  if (!this->read_bytes(MS8607_CMD_ADC_READ, bytes, 3))
    return false;
  value = encode_uint24(bytes[0], bytes[1], bytes[2]);
  return true;
}

void MS8607Component::request_read_temperature_() {
  // Tell MS8607 to start ADC conversion of temperature sensor
  if (!this->write_bytes(MS8607_CMD_CONV_D2_OSR_8K, nullptr, 0)) {
    ESP_LOGW(TAG, "Requesting ADC Read of temperature failed");
    this->update_ok_ = false;
    this->finalize_update_();
    return;
  }

  // datasheet says 17.2ms max conversion time at OSR 8192
  this->set_timeout(TEMPERATURE_TIMEOUT_ID, 20, [this]() { this->read_temperature_(); });
}

void MS8607Component::read_temperature_() {
  uint32_t d2_raw_temperature;
  if (!this->read_adc_(d2_raw_temperature)) {
    ESP_LOGW(TAG, "ADC Read of temperature failed");
    this->update_ok_ = false;
    this->finalize_update_();
    return;
  }

  const CompensatedTemperature temperature = compensated_temperature(d2_raw_temperature, this->calibration_values_);
  if (temperature.temperature_float < TEMPERATURE_LOWER_LIMIT ||
      temperature.temperature_float > TEMPERATURE_UPPER_LIMIT) {
    ESP_LOGW(TAG, "Treating this temperature read of %0.2f°C as a failed read, skipping it",
             temperature.temperature_float);
    this->update_ok_ = false;
    this->finalize_update_();
    return;
  }

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature.temperature_float);
  }

  if (this->pressure_sensor_ == nullptr || !this->request_read_pressure_(d2_raw_temperature)) {
    this->finish_update_(temperature.temperature_float);
  }
}

bool MS8607Component::request_read_pressure_(uint32_t d2_raw_temperature) {
  if (!this->write_bytes(MS8607_CMD_CONV_D1_OSR_8K, nullptr, 0)) {
    ESP_LOGW(TAG, "Requesting ADC Read of pressure failed");
    this->update_ok_ = false;
    return false;
  }

  // datasheet says 17.2ms max conversion time at OSR 8192
  this->set_timeout(PRESSURE_TIMEOUT_ID, 20,
                    [this, d2_raw_temperature]() { this->read_pressure_(d2_raw_temperature); });
  return true;
}

void MS8607Component::read_pressure_(uint32_t d2_raw_temperature) {
  const CompensatedTemperature temperature = compensated_temperature(d2_raw_temperature, this->calibration_values_);

  uint32_t d1_raw_pressure;
  if (!this->read_adc_(d1_raw_pressure)) {
    ESP_LOGW(TAG, "ADC Read of pressure failed");
    this->update_ok_ = false;
  } else if (d1_raw_pressure == 0) {
    // Sensor returns all zeros if the ADC conversion isn't done yet. Check the timeout in request_read_pressure_
    ESP_LOGW(TAG, "ADC Read of pressure returned all zeros, the conversion had not finished");
    this->update_ok_ = false;
  } else {
    const float pressure = compensated_pressure(d1_raw_pressure, this->calibration_values_, temperature);
    if (pressure < PRESSURE_LOWER_LIMIT || pressure > PRESSURE_UPPER_LIMIT) {
      ESP_LOGW(TAG, "Treating this pressure read of %.2fhPa as a failed read, skipping it", pressure);
      this->update_ok_ = false;
    } else {
      this->pressure_sensor_->publish_state(pressure);  // hPa aka mbar
    }
  }

  this->finish_update_(temperature.temperature_float);
}

void MS8607Component::finish_update_(float temperature) {
  if (this->humidity_device_ != nullptr) {
    // now that we have temperature (to compensate the humidity with), kick off that read
    this->request_read_humidity_(temperature);
  } else {
    this->finalize_update_();
  }
}

void MS8607Component::request_read_humidity_(float temperature) {
  if (!this->humidity_device_->write_bytes(MS8607_CMD_H_MEASURE_NO_HOLD, nullptr, 0)) {
    ESP_LOGW(TAG, "Request to measure humidity failed");
    this->update_ok_ = false;
    this->finalize_update_();
    return;
  }

  // datasheet says 15.89ms max conversion time at OSR 8192
  this->set_timeout(HUMIDITY_TIMEOUT_ID, 20, [this, temperature]() { this->read_humidity_(temperature); });
}

void MS8607Component::read_humidity_(float temperature) {
  uint8_t bytes[3];
  if (!this->humidity_device_->read_bytes_raw(bytes, 3)) {
    ESP_LOGW(TAG, "Failed to read the measured humidity value");
    this->update_ok_ = false;
    this->finalize_update_();
    return;
  }

  // "the measurement is stored into 14 bits. The two remaining LSBs are used for transmitting status information.
  // Bit1 of the two LSBS must be set to '1'. Bit0 is currently not assigned"
  uint16_t humidity = encode_uint16(bytes[0], bytes[1]);
  uint8_t const expected_crc = bytes[2];
  uint8_t const actual_crc = crc8(bytes, 2, 0, 0x31, true);
  if (expected_crc != actual_crc) {
    ESP_LOGW(TAG, "Incorrect Humidity CRC value. Provided value 0x%01X != calculated value 0x%01X", expected_crc,
             actual_crc);
    this->update_ok_ = false;
    this->finalize_update_();
    return;
  }
  if (!(humidity & 0x2)) {
    // data sheet says Bit1 should always set, but nothing about what happens if it isn't
    ESP_LOGW(TAG, "Humidity status bit was not set to 1?");
  }
  if (humidity == 0) {
    // treat this like a bad I2C read, because we received all zeros, despite the datasheet saying
    // there should be at least Bit1 set. CRC might have passed because all zeros results in expected CRC of 0.
    ESP_LOGW(TAG, "Treating this humidity read as a failed read, skipping it");
    this->update_ok_ = false;
    this->finalize_update_();
    return;
  }
  humidity &= ~(0b11);  // strip status & unassigned bits from data

  this->humidity_sensor_->publish_state(compensated_humidity(humidity, temperature));
  this->finalize_update_();
}

void MS8607Component::finalize_update_() { this->status_comm_result(this->update_ok_, nullptr); }

CompensatedTemperature compensated_temperature(uint32_t d2_raw_temperature,
                                               const CalibrationValues &calibration_values) {
  CompensatedTemperature result;
  // Perform the first order temperature calculation

  // d_t: "difference between actual and reference temperature" = D2 - [C5] * 2**8
  result.d_t = int32_t(d2_raw_temperature) - (int32_t(calibration_values.reference_temperature) << 8);
  const int64_t d_t_squared = int64_t(result.d_t) * result.d_t;

  // actual temperature as hundredths of degree celsius in range [-4000, 8500]
  // 2000 + d_t * [C6] / (2**23)
  result.first_order_temperature =
      2000 + ((int64_t(result.d_t) * calibration_values.temperature_coefficient_of_temperature) >> 23);

  // Perform the second order temperature compensation, for non-linearity over temperature range
  int32_t temperature_2;
  if (result.first_order_temperature < 2000) {
    // T2 = 3 * (d_t**2) / 2**33
    temperature_2 = int32_t((3 * d_t_squared) >> 33);
  } else {
    // T2 = 5 * (d_t**2) / 2**38
    temperature_2 = int32_t((5 * d_t_squared) >> 38);
  }

  result.temperature_float = (result.first_order_temperature - temperature_2) / 100.0f;

  return result;
}

float compensated_pressure(uint32_t d1_raw_pressure, const CalibrationValues &calibration_values,
                           const CompensatedTemperature &temperature_values) {
  // Perform the first order pressure calculation

  // offset at actual temperature. [C2] * (2**17) + (d_t * [C4] / (2**6))
  int64_t pressure_offset =
      (int64_t(calibration_values.pressure_offset) << 17) +
      ((int64_t(temperature_values.d_t) * calibration_values.pressure_offset_temperature_coefficient) >> 6);
  // sensitivity at actual temperature. [C1] * (2**16) + ([C3] * d_t) / (2**7)
  int64_t pressure_sensitivity =
      (int64_t(calibration_values.pressure_sensitivity) << 16) +
      ((int64_t(temperature_values.d_t) * calibration_values.pressure_sensitivity_temperature_coefficient) >> 7);

  // Perform the second order pressure compensation, for non-linearity over temperature range
  int32_t pressure_offset_2 = 0;
  int32_t pressure_sensitivity_2 = 0;
  if (temperature_values.first_order_temperature < 2000) {
    // (TEMP - 2000)**2 / 2**4
    const int32_t low_temperature_adjustment =
        (temperature_values.first_order_temperature - 2000) * (temperature_values.first_order_temperature - 2000) >> 4;

    // OFF2 = 61 * (TEMP-2000)**2 / 2**4
    pressure_offset_2 = 61 * low_temperature_adjustment;
    // SENS2 = 29 * (TEMP-2000)**2 / 2**4
    pressure_sensitivity_2 = 29 * low_temperature_adjustment;

    if (temperature_values.first_order_temperature < -1500) {
      // (TEMP+1500)**2
      const int32_t very_low_temperature_adjustment =
          (temperature_values.first_order_temperature + 1500) * (temperature_values.first_order_temperature + 1500);

      // OFF2 = OFF2 + 17 * (TEMP+1500)**2
      pressure_offset_2 += 17 * very_low_temperature_adjustment;
      // SENS2 = SENS2 + 9 * (TEMP+1500)**2
      pressure_sensitivity_2 += 9 * very_low_temperature_adjustment;
    }
  }

  pressure_offset -= pressure_offset_2;
  pressure_sensitivity -= pressure_sensitivity_2;

  // Temperature compensated pressure. [1000, 120000] => [10.00 mbar, 1200.00 mbar]
  const int32_t pressure = (((d1_raw_pressure * pressure_sensitivity) >> 21) - pressure_offset) >> 15;

  return pressure / 100.0f;
}

float compensated_humidity(float humidity_float, float temperature_float) {
  // map 16 bit humidity value into range [-6%, 119%]
  // Datasheet is inconsistent on upper max value (118 vs 119), but example value,
  // calculation equation, and max value in table all indicate it should be 119%
  float const humidity_partial = humidity_float / (1 << 16);
  float const humidity_percentage = std::lerp(-6.0f, 119.0f, humidity_partial);
  return humidity_percentage + (20 - temperature_float) * MS8607_H_TEMP_COEFFICIENT;
}

}  // namespace esphome::ms8607
