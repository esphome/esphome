/*
  based on BMP388_DEV by Martin Lindupp
  under MIT License (MIT)
  Copyright (C) Martin Lindupp 2020
  http://github.com/MartinL1/BMP388_DEV
*/

#include "bmp3xx_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cinttypes>

namespace esphome::bmp3xx_base {

ESPHOME_LOG_TAG(TAG, "bmp3xx.sensor");

static constexpr uint32_t DATA_TIMEOUT_ID = 0;
static constexpr uint8_t ERR_REG_CONF_ERR = 0x04;  // conf_err bit of BMP388_ERR_REG

static const LogString *chip_type_to_str(uint8_t chip_type) {
  switch (chip_type) {
    case BMP388_ID:
      return LOG_STR("BMP 388");
    case BMP390_ID:
      return LOG_STR("BMP 390");
    default:
      return LOG_STR("Unknown Chip Type");
  }
}

// Oversampling strings indexed by Oversampling enum (0-5): NONE, X2, X4, X8, X16, X32
PROGMEM_STRING_TABLE(OversamplingStrings, "None", "2x", "4x", "8x", "16x", "32x", "");

static const LogString *oversampling_to_str(Oversampling oversampling) {
  return OversamplingStrings::get_log_str(static_cast<uint8_t>(oversampling), OversamplingStrings::LAST_INDEX);
}

// IIR filter strings indexed by IIRFilter enum (0-7): OFF, 2, 4, 8, 16, 32, 64, 128
PROGMEM_STRING_TABLE(IIRFilterStrings, "OFF", "2x", "4x", "8x", "16x", "32x", "64x", "128x", "");

static const LogString *iir_filter_to_str(IIRFilter filter) {
  return IIRFilterStrings::get_log_str(static_cast<uint8_t>(filter), IIRFilterStrings::LAST_INDEX);
}

void BMP3XXComponent::setup() {
  // Call the Device base class "initialise" function
  if (!reset()) {
    this->mark_failed(LOG_STR("Failed to reset"));
    return;
  }

  if (!read_byte(BMP388_CHIP_ID, &this->chip_id_.reg)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  ESP_LOGCONFIG(TAG, "Chip %s Id 0x%X", LOG_STR_ARG(chip_type_to_str(this->chip_id_.reg)), this->chip_id_.reg);

  if (chip_id_.reg != BMP388_ID && chip_id_.reg != BMP390_ID) {
    this->mark_failed(LOG_STR("Wrong chip ID - please check if you are really using a BMP 388 or BMP 390"));
    return;
  }
  // set sensor in sleep mode
  stop_conversion();
  // Read the calibration parameters into the params structure
  if (!read_bytes(BMP388_TRIM_PARAMS, (uint8_t *) &compensation_params_, sizeof(compensation_params_))) {
    this->mark_failed(LOG_STR("Can't read calibration data"));
    return;
  }
  compensation_float_params_.param_T1 =
      (float) compensation_params_.param_T1 / powf(2.0f, -8.0f);  // Calculate the floating point trim parameters
  compensation_float_params_.param_T2 = (float) compensation_params_.param_T2 / powf(2.0f, 30.0f);
  compensation_float_params_.param_T3 = (float) compensation_params_.param_T3 / powf(2.0f, 48.0f);
  compensation_float_params_.param_P1 = ((float) compensation_params_.param_P1 - powf(2.0f, 14.0f)) / powf(2.0f, 20.0f);
  compensation_float_params_.param_P2 = ((float) compensation_params_.param_P2 - powf(2.0f, 14.0f)) / powf(2.0f, 29.0f);
  compensation_float_params_.param_P3 = (float) compensation_params_.param_P3 / powf(2.0f, 32.0f);
  compensation_float_params_.param_P4 = (float) compensation_params_.param_P4 / powf(2.0f, 37.0f);
  compensation_float_params_.param_P5 = (float) compensation_params_.param_P5 / powf(2.0f, -3.0f);
  compensation_float_params_.param_P6 = (float) compensation_params_.param_P6 / powf(2.0f, 6.0f);
  compensation_float_params_.param_P7 = (float) compensation_params_.param_P7 / powf(2.0f, 8.0f);
  compensation_float_params_.param_P8 = (float) compensation_params_.param_P8 / powf(2.0f, 15.0f);
  compensation_float_params_.param_P9 = (float) compensation_params_.param_P9 / powf(2.0f, 48.0f);
  compensation_float_params_.param_P10 = (float) compensation_params_.param_P10 / powf(2.0f, 48.0f);
  compensation_float_params_.param_P11 = (float) compensation_params_.param_P11 / powf(2.0f, 65.0f);

  // Set power control registers
  pwr_ctrl_.bit.press_en = 1;
  pwr_ctrl_.bit.temp_en = 1;
  // Disable pressure if no sensor defined
  // keep temperature enabled since it's needed for compensation
  if (this->pressure_sensor_ == nullptr) {
    pwr_ctrl_.bit.press_en = 0;
    this->pressure_oversampling_ = OVERSAMPLING_NONE;
  }
  // just disable oeversampling for temp if not used
  if (this->temperature_sensor_ == nullptr) {
    this->temperature_oversampling_ = OVERSAMPLING_NONE;
  }
  if (!this->configure_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  if (this->operation_mode_ == NORMAL_MODE) {
    uint8_t err_reg;
    if (!this->read_byte(BMP388_ERR_REG, &err_reg)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
    // The sensor stays asleep and sets conf_err if one measurement takes longer than the data rate period.
    // _validate_data_rate rejects that at config time; this guards against its timing formula drifting.
    if (err_reg & ERR_REG_CONF_ERR) {
      this->mark_failed(LOG_STR("Data rate too fast for the oversampling settings"));
      return;
    }
  }
}

void BMP3XXComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BMP3XX:\n"
                "  Type: %s (0x%X)",
                LOG_STR_ARG(chip_type_to_str(this->chip_id_.reg)), this->chip_id_.reg);
  ESP_LOGCONFIG(TAG, "  IIR Filter: %s", LOG_STR_ARG(iir_filter_to_str(this->iir_filter_)));
  if (this->operation_mode_ == NORMAL_MODE) {
    // Each data rate step doubles the period, starting from 5 ms at 200 Hz
    ESP_LOGCONFIG(TAG, "  Data Rate Period: %" PRIu32 " ms", UINT32_C(5) << this->odr_.bit.odr_sel);
  }
  LOG_UPDATE_INTERVAL(this);
  if (this->temperature_sensor_) {
    LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
    ESP_LOGCONFIG(TAG, "    Oversampling: %s", LOG_STR_ARG(oversampling_to_str(this->temperature_oversampling_)));
  }
  if (this->pressure_sensor_) {
    LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
    ESP_LOGCONFIG(TAG, "    Oversampling: %s", LOG_STR_ARG(oversampling_to_str(this->pressure_oversampling_)));
  }
}

inline uint8_t oversampling_to_time(Oversampling over_sampling) { return (1 << uint8_t(over_sampling)); }

bool BMP3XXComponent::configure_() {
  if (!this->set_iir_filter(this->iir_filter_) ||
      !this->set_oversampling_register(this->pressure_oversampling_, this->temperature_oversampling_)) {
    return false;
  }
  if (this->operation_mode_ != NORMAL_MODE)
    return true;
  return this->write_byte(BMP388_ODR, this->odr_.reg) && this->set_mode(NORMAL_MODE);
}

void BMP3XXComponent::update() {
  // In NORMAL_MODE the sensor is already measuring; the data registers hold the latest result
  if (this->operation_mode_ == NORMAL_MODE) {
    this->read_and_publish_();
    return;
  }
  ESP_LOGV(TAG, "Sending conversion request");
  float meas_time = 1.0f;
  // Ref: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp390-ds002.pdf 3.9.2
  // Keep in step with _validate_data_rate in __init__.py
  meas_time += 2.02f * oversampling_to_time(this->temperature_oversampling_) + 0.163f;
  meas_time += 2.02f * oversampling_to_time(this->pressure_oversampling_) + 0.392f;
  meas_time += 0.234f;
  if (!set_mode(FORCED_MODE)) {
    this->mark_failed(LOG_STR("Failed start forced mode"));
    return;
  }

  const uint32_t meas_timeout = uint32_t(ceilf(meas_time));
  ESP_LOGVV(TAG, "measurement time %" PRIu32, meas_timeout);
  this->set_timeout(DATA_TIMEOUT_ID, meas_timeout, [this]() { this->read_and_publish_(); });
}

void BMP3XXComponent::read_and_publish_() {
  float temperature = 0.0f;
  float pressure = 0.0f;
  if (this->pressure_sensor_ != nullptr) {
    if (!get_measurements(temperature, pressure)) {
      ESP_LOGW(TAG, "Failed to read pressure and temperature - skipping update");
      this->status_set_warning();
      return;
    }
    ESP_LOGD(TAG, "Got temperature=%.1f°C pressure=%.1fhPa", temperature, pressure);
  } else {
    if (!get_temperature(temperature)) {
      ESP_LOGW(TAG, "Failed to read temperature - skipping update");
      this->status_set_warning();
      return;
    }
    ESP_LOGD(TAG, "Got temperature=%.1f°C", temperature);
  }

  PUBLISH_STATE(this->temperature_sensor_, temperature);
  PUBLISH_STATE(this->pressure_sensor_, pressure);
  this->status_clear_warning();
  if (this->operation_mode_ == FORCED_MODE)
    set_mode(SLEEP_MODE);
}

// Reset the BMP3XX
uint8_t BMP3XXComponent::reset() {
  write_byte(BMP388_CMD, RESET_CODE);  // Write the reset code to the command register
  // Wait for 10ms
  delay(10);
  this->read_byte(BMP388_EVENT, &event_.reg);  // Read the BMP388's event register
  return event_.bit.por_detected;              // Return if device reset is complete
}

// Start a one shot measurement in FORCED_MODE
bool BMP3XXComponent::start_forced_conversion() {
  // Only set FORCED_MODE if we're already in SLEEP_MODE
  if (pwr_ctrl_.bit.mode == SLEEP_MODE) {
    return set_mode(FORCED_MODE);
  }
  return true;
}

// Stop the conversion and return to SLEEP_MODE
bool BMP3XXComponent::stop_conversion() { return set_mode(SLEEP_MODE); }

// Set the pressure oversampling rate
bool BMP3XXComponent::set_pressure_oversampling(Oversampling oversampling) {
  osr_.bit.osr_p = oversampling;
  return this->write_byte(BMP388_OSR, osr_.reg);
}

// Set the temperature oversampling rate
bool BMP3XXComponent::set_temperature_oversampling(Oversampling oversampling) {
  osr_.bit.osr_t = oversampling;
  return this->write_byte(BMP388_OSR, osr_.reg);
}

// Set the IIR filter setting
bool BMP3XXComponent::set_iir_filter(IIRFilter iir_filter) {
  config_.bit.iir_filter = iir_filter;
  return this->write_byte(BMP388_CONFIG, config_.reg);
}

// Get temperature
bool BMP3XXComponent::get_temperature(float &temperature) {
  // Check if a measurement is ready
  if (!data_ready()) {
    return false;
  }
  uint8_t data[3];
  // Read the temperature
  if (!this->read_bytes(BMP388_DATA_3, &data[0], 3)) {
    ESP_LOGE(TAG, "Failed to read temperature");
    return false;
  }
  // Copy the temperature data into the adc variables
  int32_t adc_temp = encode_uint24(data[2], data[1], data[0]);
  // Temperature compensation (function from BMP388 datasheet)
  temperature = bmp388_compensate_temperature_((float) adc_temp);
  return true;
}

// Get the pressure
bool BMP3XXComponent::get_pressure(float &pressure) {
  float temperature;
  return get_measurements(temperature, pressure);
}

// Get temperature and pressure
bool BMP3XXComponent::get_measurements(float &temperature, float &pressure) {
  // Check if a measurement is ready
  if (!data_ready()) {
    ESP_LOGD(TAG, "Get measurement - data not ready skipping update");
    return false;
  }

  uint8_t data[6];
  // Read the temperature and pressure data
  if (!this->read_bytes(BMP388_DATA_0, &data[0], 6)) {
    ESP_LOGE(TAG, "Failed to read measurements");
    return false;
  }
  // Copy the temperature and pressure data into the adc variables
  int32_t adc_pres = encode_uint24(data[2], data[1], data[0]);
  int32_t adc_temp = encode_uint24(data[5], data[4], data[3]);

  // Temperature compensation (function from BMP388 datasheet)
  temperature = bmp388_compensate_temperature_((float) adc_temp);
  // Pressure compensation (function from BMP388 datasheet)
  pressure = bmp388_compensate_pressure_((float) adc_pres, temperature);
  // Calculate the pressure in millibar/hPa
  pressure /= 100.0f;
  return true;
}

// Set the BMP388's mode in the power control register
bool BMP3XXComponent::set_mode(OperationMode mode) {
  pwr_ctrl_.bit.mode = mode;
  return this->write_byte(BMP388_PWR_CTRL, pwr_ctrl_.reg);
}

// Set the BMP388 oversampling register
bool BMP3XXComponent::set_oversampling_register(Oversampling pressure_oversampling,
                                                Oversampling temperature_oversampling) {
  osr_.reg = temperature_oversampling << 3 | pressure_oversampling;
  return this->write_byte(BMP388_OSR, osr_.reg);
}

// Check if measurement data is ready
bool BMP3XXComponent::data_ready() {
  // If we're in SLEEP_MODE return immediately
  if (pwr_ctrl_.bit.mode == SLEEP_MODE) {
    ESP_LOGD(TAG, "Not ready - sensor is in sleep mode");
    return false;
  }
  if (this->operation_mode_ == NORMAL_MODE) {
    // A sensor that lost power comes back asleep with default settings, and its data would never change
    decltype(this->pwr_ctrl_) pwr_ctrl;
    if (!this->read_byte(BMP388_PWR_CTRL, &pwr_ctrl.reg)) {
      ESP_LOGE(TAG, "Failed to read power control register");
      return false;
    }
    if (pwr_ctrl.bit.mode != NORMAL_MODE) {
      ESP_LOGW(TAG, "Sensor was reset - restoring its settings");
      this->normal_mode_data_valid_ = false;
      if (!this->configure_()) {
        ESP_LOGE(TAG, "Failed to restore settings");
      }
      return false;
    }
    // The data registers always hold the latest result once the first one is in.
    // drdy clears on read, so it would fail a poll that comes before the next result.
    if (this->normal_mode_data_valid_) {
      return true;
    }
  }
  // Read the interrupt status register
  uint8_t status;
  if (!this->read_byte(BMP388_INT_STATUS, &status)) {
    ESP_LOGE(TAG, "Failed to read status register");
    return false;
  }
  int_status_.reg = status;
  ESP_LOGVV(TAG, "data ready status %d", status);
  // If we're in FORCED_MODE switch back to SLEEP_MODE
  if (int_status_.bit.drdy) {
    if (pwr_ctrl_.bit.mode == FORCED_MODE) {
      pwr_ctrl_.bit.mode = SLEEP_MODE;
    }
    // Only read in NORMAL_MODE
    this->normal_mode_data_valid_ = true;
    return true;  // The measurement is ready
  }
  return false;  // The measurement is still pending
}

////////////////////////////////////////////////////////////////////////////////
// Bosch BMP3XXComponent (Private) Member Functions
////////////////////////////////////////////////////////////////////////////////

float BMP3XXComponent::bmp388_compensate_temperature_(float uncomp_temp) {
  float partial_data1 = uncomp_temp - compensation_float_params_.param_T1;
  float partial_data2 = partial_data1 * compensation_float_params_.param_T2;
  return partial_data2 + partial_data1 * partial_data1 * compensation_float_params_.param_T3;
}

float BMP3XXComponent::bmp388_compensate_pressure_(float uncomp_press, float t_lin) {
  float partial_data1 = compensation_float_params_.param_P6 * t_lin;
  float partial_data2 = compensation_float_params_.param_P7 * t_lin * t_lin;
  float partial_data3 = compensation_float_params_.param_P8 * t_lin * t_lin * t_lin;
  float partial_out1 = compensation_float_params_.param_P5 + partial_data1 + partial_data2 + partial_data3;
  partial_data1 = compensation_float_params_.param_P2 * t_lin;
  partial_data2 = compensation_float_params_.param_P3 * t_lin * t_lin;
  partial_data3 = compensation_float_params_.param_P4 * t_lin * t_lin * t_lin;
  float partial_out2 =
      uncomp_press * (compensation_float_params_.param_P1 + partial_data1 + partial_data2 + partial_data3);
  partial_data1 = uncomp_press * uncomp_press;
  partial_data2 = compensation_float_params_.param_P9 + compensation_float_params_.param_P10 * t_lin;
  partial_data3 = partial_data1 * partial_data2;
  float partial_data4 =
      partial_data3 + uncomp_press * uncomp_press * uncomp_press * compensation_float_params_.param_P11;
  return partial_out1 + partial_out2 + partial_data4;
}

}  // namespace esphome::bmp3xx_base
