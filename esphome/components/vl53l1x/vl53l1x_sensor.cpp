// VL53L1X Time-of-Flight Distance Sensor Driver
// Based on Pololu VL53L1X library: https://github.com/pololu/vl53l1x-arduino
// Which is based on ST's Ultra Lite Driver

#include "vl53l1x_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace vl53l1x {

static const char *const TAG = "vl53l1x";

// Default tuning values from Pololu/ST
static const uint8_t DEFAULT_CONFIG[] = {
    // From VL53L1_DataInit
    0x00,  // 0x2D: PAD_I2C_HV__EXTSUP_CONFIG (will be modified for 2.8V)
    0x01,  // 0x2E: reserved
    0x01,  // 0x2F: reserved
    0x01,  // 0x30: GPIO_HV_MUX__CTRL
    0x02,  // 0x31: reserved
    0x00,  // 0x32: reserved
    0x02,  // 0x33: reserved
    0x08,  // 0x34: reserved
    0x00,  // 0x35: reserved
    0x08,  // 0x36: reserved
    0x10,  // 0x37: reserved
    0x01,  // 0x38: reserved
    0x01,  // 0x39: reserved
    0x00,  // 0x3A: reserved
    0x00,  // 0x3B: reserved
    0x00,  // 0x3C: reserved
    0x00,  // 0x3D: reserved
    0xFF,  // 0x3E: reserved
    0x00,  // 0x3F: reserved
    0x0F,  // 0x40: reserved
    0x00,  // 0x41: reserved
    0x00,  // 0x42: reserved
    0x00,  // 0x43: reserved
    0x00,  // 0x44: reserved
    0x00,  // 0x45: reserved
    0x20,  // 0x46: CAL_CONFIG__SPAD_DC__TARGET_COUNT
    0x0B,  // 0x47: CAL_CONFIG__VCSEL_START
    0x00,  // 0x48: reserved
    0x00,  // 0x49: reserved
    0x02,  // 0x4A: reserved
    0x14,  // 0x4B: PHASECAL_CONFIG__TIMEOUT_MACROP
    0x21,  // 0x4C: reserved
    0x00,  // 0x4D: PHASECAL_CONFIG__OVERRIDE
    0x00,  // 0x4E: reserved
    0x05,  // 0x4F: DSS_CONFIG__ROI_MODE_CONTROL
    0x00,  // 0x50: SYSTEM__THRESH_RATE_HIGH
    0x00,  // 0x51: SYSTEM__THRESH_RATE_HIGH
    0x00,  // 0x52: SYSTEM__THRESH_RATE_LOW
    0x00,  // 0x53: SYSTEM__THRESH_RATE_LOW
    0xC8,  // 0x54: DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT (200 << 8)
    0x00,  // 0x55: DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT
    0x00,  // 0x56: reserved
    0x38,  // 0x57: DSS_CONFIG__APERTURE_ATTENUATION
    0xFF,  // 0x58: reserved
    0x01,  // 0x59: reserved
    0x00,  // 0x5A: reserved
    0x08,  // 0x5B: reserved
    0x00,  // 0x5C: reserved
    0x00,  // 0x5D: reserved
    0x01,  // 0x5E: RANGE_CONFIG__TIMEOUT_MACROP_A_HI
    0xCC,  // 0x5F: RANGE_CONFIG__TIMEOUT_MACROP_A_LO
    0x0F,  // 0x60: RANGE_CONFIG__VCSEL_PERIOD_A
    0x01,  // 0x61: RANGE_CONFIG__TIMEOUT_MACROP_B_HI
    0xF1,  // 0x62: RANGE_CONFIG__TIMEOUT_MACROP_B_LO
    0x0D,  // 0x63: RANGE_CONFIG__VCSEL_PERIOD_B
    0x01,  // 0x64: RANGE_CONFIG__SIGMA_THRESH
    0x68,  // 0x65: RANGE_CONFIG__SIGMA_THRESH (360)
    0x00,  // 0x66: RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS
    0x80,  // 0x67: RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS (0.5 MCPS)
    0x08,  // 0x68: reserved
    0xB8,  // 0x69: RANGE_CONFIG__VALID_PHASE_HIGH
    0x00,  // 0x6A: reserved
    0x00,  // 0x6B: reserved
    0x00,  // 0x6C: SYSTEM__INTERMEASUREMENT_PERIOD
    0x00,  // 0x6D: SYSTEM__INTERMEASUREMENT_PERIOD
    0x0F,  // 0x6E: SYSTEM__INTERMEASUREMENT_PERIOD
    0x89,  // 0x6F: SYSTEM__INTERMEASUREMENT_PERIOD
    0x00,  // 0x70: reserved
    0x00,  // 0x71: SYSTEM__THRESH_HIGH
    0x00,  // 0x72: SYSTEM__THRESH_HIGH
    0x00,  // 0x73: SYSTEM__THRESH_LOW
    0x00,  // 0x74: SYSTEM__THRESH_LOW
    0x00,  // 0x75: reserved
    0x00,  // 0x76: reserved
    0x01,  // 0x77: reserved
    0x0F,  // 0x78: SD_CONFIG__WOI_SD0
    0x0D,  // 0x79: SD_CONFIG__WOI_SD1
    0x0E,  // 0x7A: SD_CONFIG__INITIAL_PHASE_SD0
    0x0E,  // 0x7B: SD_CONFIG__INITIAL_PHASE_SD1
    0x00,  // 0x7C: reserved
    0x00,  // 0x7D: reserved
    0x02,  // 0x7E: reserved
    0xC7,  // 0x7F: ROI_CONFIG__USER_ROI_CENTRE_SPAD
    0xFF,  // 0x80: ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE
    0x9B,  // 0x81: SYSTEM__SEQUENCE_CONFIG
    0x00,  // 0x82: reserved
    0x00,  // 0x83: reserved
    0x00,  // 0x84: reserved
    0x01,  // 0x85: reserved
    0x00,  // 0x86: SYSTEM__INTERRUPT_CLEAR
    0x00,  // 0x87: SYSTEM__MODE_START
};

void VL53L1XSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VL53L1X...");

  // If enable pin is configured, use it to control the sensor
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(false);
    delay(2);  // Short delay while sensor is held in reset
    this->enable_pin_->digital_write(true);
    delay(2);  // Wait for sensor to boot
  }

  // Initialize the sensor
  if (!this->init_sensor_()) {
    ESP_LOGE(TAG, "Failed to initialize VL53L1X");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "VL53L1X initialized successfully");
}

void VL53L1XSensor::loop() {
  if (this->state_ == MEASUREMENT_STATE_IDLE) {
    return;
  }

  // Check for timeout
  if (micros() - this->measurement_start_us_ > this->timeout_us_) {
    ESP_LOGW(TAG, "Measurement timeout");
    this->status_set_warning();
    this->state_ = MEASUREMENT_STATE_IDLE;
    this->publish_state(NAN);
    return;
  }

  // Check if data is ready
  if (!this->data_ready_()) {
    return;  // Not ready yet, check again next loop
  }

  // Read and process results
  if (!this->read_results_()) {
    ESP_LOGW(TAG, "Failed to read results");
    this->status_set_warning();
    this->publish_state(NAN);
  }

  this->state_ = MEASUREMENT_STATE_IDLE;
}

void VL53L1XSensor::update() {
  if (this->is_failed()) {
    return;
  }

  if (this->state_ != MEASUREMENT_STATE_IDLE) {
    ESP_LOGW(TAG, "Measurement already in progress");
    return;
  }

  if (!this->start_measurement_()) {
    ESP_LOGW(TAG, "Failed to start measurement");
    this->status_set_warning();
    return;
  }

  this->measurement_start_us_ = micros();
  this->state_ = MEASUREMENT_STATE_WAITING_FOR_DATA;
}

void VL53L1XSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "VL53L1X:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
    return;
  }
  ESP_LOGCONFIG(TAG, "  Distance Mode: %s",
                this->distance_mode_ == DISTANCE_MODE_SHORT    ? "Short"
                : this->distance_mode_ == DISTANCE_MODE_MEDIUM ? "Medium"
                                                               : "Long");
  ESP_LOGCONFIG(TAG, "  Timing Budget: %u us", this->timing_budget_us_);
  ESP_LOGCONFIG(TAG, "  ROI: %dx%d, center=%d", this->roi_width_, this->roi_height_, this->roi_center_);
  if (this->offset_mm_ != 0) {
    ESP_LOGCONFIG(TAG, "  Offset: %d mm", this->offset_mm_);
  }
  if (this->enable_pin_ != nullptr) {
    LOG_PIN("  Enable Pin: ", this->enable_pin_);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Distance", this);
}

void VL53L1XSensor::set_roi(uint8_t width, uint8_t height, uint8_t center) {
  this->roi_width_ = width;
  this->roi_height_ = height;
  this->roi_center_ = center;
}

// --- Initialization ---

bool VL53L1XSensor::init_sensor_() {
  // Wait for device to boot
  if (!this->wait_for_boot_()) {
    ESP_LOGE(TAG, "Device did not boot");
    return false;
  }

  // Verify model ID
  uint16_t model_id;
  if (!this->read_reg16_(IDENTIFICATION_MODEL_ID, &model_id)) {
    ESP_LOGE(TAG, "Failed to read model ID");
    return false;
  }
  if (model_id != 0xEACC) {
    ESP_LOGE(TAG, "Wrong model ID: 0x%04X (expected 0xEACC)", model_id);
    return false;
  }

  // Soft reset
  if (!this->write_reg_(SOFT_RESET, 0x00)) {
    return false;
  }
  delay(1);
  if (!this->write_reg_(SOFT_RESET, 0x01)) {
    return false;
  }
  delay(1);

  // Wait for boot after reset
  if (!this->wait_for_boot_()) {
    ESP_LOGE(TAG, "Device did not boot after reset");
    return false;
  }

  // Read oscillator info for timing calculations
  if (!this->read_reg16_(OSC_MEASURED_FAST_OSC_FREQUENCY, &this->fast_osc_frequency_)) {
    return false;
  }
  if (!this->read_reg16_(RESULT_OSC_CALIBRATE_VAL, &this->osc_calibrate_val_)) {
    return false;
  }

  // Apply default configuration
  if (!this->configure_sensor_()) {
    return false;
  }

  // Apply user configuration
  if (!this->set_distance_mode_internal_(this->distance_mode_)) {
    ESP_LOGE(TAG, "Failed to set distance mode");
    return false;
  }

  if (!this->set_timing_budget_internal_(this->timing_budget_us_)) {
    ESP_LOGE(TAG, "Failed to set timing budget");
    return false;
  }

  if (!this->set_roi_internal_()) {
    ESP_LOGE(TAG, "Failed to set ROI");
    return false;
  }

  if (!this->set_offset_internal_()) {
    ESP_LOGE(TAG, "Failed to set offset");
    return false;
  }

  return true;
}

bool VL53L1XSensor::wait_for_boot_() {
  uint32_t start = millis();
  while (millis() - start < 100) {
    uint8_t status;
    if (this->read_reg_(FIRMWARE_SYSTEM_STATUS, &status) && (status & 0x01)) {
      return true;
    }
    delay(1);
  }
  return false;
}

bool VL53L1XSensor::configure_sensor_() {
  // Write default tuning values starting at register 0x2D
  // Write in smaller chunks for better reliability
  for (size_t i = 0; i < sizeof(DEFAULT_CONFIG); i++) {
    if (!this->write_reg_(0x2D + i, DEFAULT_CONFIG[i])) {
      ESP_LOGE(TAG, "Failed to write config at offset %d", i);
      return false;
    }
  }

  // Configure for 2.8V I/O
  uint8_t extsup;
  if (!this->read_reg_(PAD_I2C_HV_EXTSUP_CONFIG, &extsup)) {
    return false;
  }
  if (!this->write_reg_(PAD_I2C_HV_EXTSUP_CONFIG, extsup | 0x01)) {
    return false;
  }

  // Set interrupt polarity to active low
  uint8_t gpio_hv_mux;
  if (!this->read_reg_(GPIO_HV_MUX_CTRL, &gpio_hv_mux)) {
    return false;
  }
  if (!this->write_reg_(GPIO_HV_MUX_CTRL, gpio_hv_mux | 0x10)) {
    return false;
  }

  // Clear any pending interrupts
  this->clear_interrupt_();

  return true;
}

bool VL53L1XSensor::set_distance_mode_internal_(DistanceMode mode) {
  // VCSEL period and timeout values for each mode
  uint8_t vcsel_a, vcsel_b;
  uint8_t woi_sd0, woi_sd1;
  uint8_t phase_sd0, phase_sd1;
  uint8_t valid_phase;

  switch (mode) {
    case DISTANCE_MODE_SHORT:
      vcsel_a = 0x07;  // 14 PCLKs
      vcsel_b = 0x05;  // 10 PCLKs
      woi_sd0 = 0x07;
      woi_sd1 = 0x05;
      phase_sd0 = 0x06;
      phase_sd1 = 0x06;
      valid_phase = 0x38;
      break;
    case DISTANCE_MODE_MEDIUM:
      vcsel_a = 0x0B;  // 22 PCLKs
      vcsel_b = 0x09;  // 18 PCLKs
      woi_sd0 = 0x0B;
      woi_sd1 = 0x09;
      phase_sd0 = 0x0A;
      phase_sd1 = 0x0A;
      valid_phase = 0x78;
      break;
    case DISTANCE_MODE_LONG:
    default:
      vcsel_a = 0x0F;  // 30 PCLKs
      vcsel_b = 0x0D;  // 26 PCLKs
      woi_sd0 = 0x0F;
      woi_sd1 = 0x0D;
      phase_sd0 = 0x0E;
      phase_sd1 = 0x0E;
      valid_phase = 0xB8;
      break;
  }

  if (!this->write_reg_(RANGE_CONFIG_VCSEL_PERIOD_A, vcsel_a))
    return false;
  if (!this->write_reg_(RANGE_CONFIG_VCSEL_PERIOD_B, vcsel_b))
    return false;
  if (!this->write_reg_(SD_CONFIG_WOI_SD0, woi_sd0))
    return false;
  if (!this->write_reg_(SD_CONFIG_WOI_SD1, woi_sd1))
    return false;
  if (!this->write_reg_(SD_CONFIG_INITIAL_PHASE_SD0, phase_sd0))
    return false;
  if (!this->write_reg_(SD_CONFIG_INITIAL_PHASE_SD1, phase_sd1))
    return false;
  if (!this->write_reg_(RANGE_CONFIG_VALID_PHASE_HIGH, valid_phase))
    return false;

  this->distance_mode_ = mode;
  return true;
}

bool VL53L1XSensor::set_timing_budget_internal_(uint32_t budget_us) {
  // Minimum timing budgets per mode
  uint32_t min_budget;
  switch (this->distance_mode_) {
    case DISTANCE_MODE_SHORT:
      min_budget = 15000;  // 15ms
      break;
    case DISTANCE_MODE_MEDIUM:
      min_budget = 20000;  // 20ms
      break;
    case DISTANCE_MODE_LONG:
    default:
      min_budget = 33000;  // 33ms
      break;
  }

  if (budget_us < min_budget) {
    ESP_LOGW(TAG, "Timing budget %u us too low for mode, using minimum %u us", budget_us, min_budget);
    budget_us = min_budget;
  }

  // Calculate timeout values
  // The timing budget is split between range A and range B
  uint32_t range_time_us = budget_us - TIMING_GUARD_US;

  // Read current VCSEL periods
  uint8_t vcsel_a, vcsel_b;
  if (!this->read_reg_(RANGE_CONFIG_VCSEL_PERIOD_A, &vcsel_a))
    return false;
  if (!this->read_reg_(RANGE_CONFIG_VCSEL_PERIOD_B, &vcsel_b))
    return false;

  // Calculate macro periods
  uint32_t macro_period_a = this->calc_macro_period_(vcsel_a);
  uint32_t macro_period_b = this->calc_macro_period_(vcsel_b);

  // Split timing budget equally between A and B
  uint32_t timeout_a_us = range_time_us / 2;
  uint32_t timeout_b_us = range_time_us / 2;

  // Convert to macro clock ticks and encode
  uint32_t mclks_a = this->timeout_microseconds_to_mclks_(timeout_a_us, macro_period_a);
  uint32_t mclks_b = this->timeout_microseconds_to_mclks_(timeout_b_us, macro_period_b);

  uint16_t encoded_a = this->encode_timeout_(mclks_a);
  uint16_t encoded_b = this->encode_timeout_(mclks_b);

  if (!this->write_reg16_(RANGE_CONFIG_TIMEOUT_MACROP_A_HI, encoded_a))
    return false;
  if (!this->write_reg16_(RANGE_CONFIG_TIMEOUT_MACROP_B_HI, encoded_b))
    return false;

  this->timing_budget_us_ = budget_us;
  return true;
}

bool VL53L1XSensor::set_roi_internal_() {
  // ROI is specified as width x height, with center at roi_center_
  // The SPAD array is 16x16
  // Encode as: ((height - 1) << 4) | (width - 1)
  uint8_t xy_size = ((this->roi_height_ - 1) << 4) | (this->roi_width_ - 1);

  if (!this->write_reg_(ROI_CONFIG_USER_ROI_CENTRE_SPAD, this->roi_center_))
    return false;
  if (!this->write_reg_(ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE, xy_size))
    return false;

  return true;
}

bool VL53L1XSensor::set_offset_internal_() {
  // Offset is stored as mm * 4 (Q2.14 fixed point for inner/outer)
  int16_t offset_q = this->offset_mm_ * 4;

  if (!this->write_reg16_(ALGO_PART_TO_PART_RANGE_OFFSET_MM, offset_q))
    return false;
  if (!this->write_reg16_(MM_CONFIG_INNER_OFFSET_MM, 0))
    return false;
  if (!this->write_reg16_(MM_CONFIG_OUTER_OFFSET_MM, 0))
    return false;

  return true;
}

// --- Measurement ---

bool VL53L1XSensor::start_measurement_() {
  // Clear any pending interrupt
  this->clear_interrupt_();

  // Start single-shot measurement
  return this->write_reg_(SYSTEM_MODE_START, 0x10);
}

bool VL53L1XSensor::data_ready_() {
  uint8_t gpio_status;
  if (!this->read_reg_(GPIO_TIO_HV_STATUS, &gpio_status)) {
    return false;
  }
  // Interrupt is active low (due to GPIO_HV_MUX setting)
  // Bit 0 indicates the interrupt state
  return (gpio_status & 0x01) == 0;
}

bool VL53L1XSensor::read_results_() {
  // Read result registers (17 bytes starting at RESULT_RANGE_STATUS)
  uint8_t buffer[17];
  uint8_t reg_addr[2] = {static_cast<uint8_t>(RESULT_RANGE_STATUS >> 8),
                         static_cast<uint8_t>(RESULT_RANGE_STATUS & 0xFF)};

  if (this->write(reg_addr, 2) != i2c::ERROR_OK) {
    return false;
  }
  if (this->read(buffer, 17) != i2c::ERROR_OK) {
    return false;
  }

  // Parse results
  uint8_t range_status = buffer[0];
  // Effective SPADs for DSS calculation
  // uint16_t effective_spads = (buffer[3] << 8) | buffer[4];
  // Ambient rate
  // uint16_t ambient_rate = (buffer[7] << 8) | buffer[8];
  // Crosstalk-corrected peak signal rate
  // uint16_t peak_signal_rate = (buffer[15] << 8) | buffer[16];
  // Final range in mm
  uint16_t range_mm = (buffer[13] << 8) | buffer[14];

  // Clear interrupt
  this->clear_interrupt_();

  // Stop measurement
  this->write_reg_(SYSTEM_MODE_START, 0x00);

  // Check range status
  RangeStatus status = static_cast<RangeStatus>(range_status >> 5);

  if (status != RangeStatus::RANGE_VALID && status != RangeStatus::RANGE_VALID_MIN_RANGE_CLIPPED &&
      status != RangeStatus::RANGE_VALID_MERGED_PULSE) {
    ESP_LOGD(TAG, "Range status: %d", static_cast<int>(status));
    this->status_set_warning();
    this->publish_state(NAN);
    return true;  // Not a read error, just invalid range
  }

  // Convert to meters with gain correction
  // The sensor reports range * 2048 / 2011 according to Pololu
  float range_m = (range_mm * 2011.0f / 2048.0f) / 1000.0f;

  this->status_clear_warning();
  this->publish_state(range_m);
  return true;
}

void VL53L1XSensor::clear_interrupt_() { this->write_reg_(SYSTEM_INTERRUPT_CLEAR, 0x01); }

// --- I2C Helpers ---

bool VL53L1XSensor::write_reg_(uint16_t reg, uint8_t value) {
  uint8_t data[3] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF), value};
  return this->write(data, 3) == i2c::ERROR_OK;
}

bool VL53L1XSensor::write_reg16_(uint16_t reg, uint16_t value) {
  uint8_t data[4] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF), static_cast<uint8_t>(value >> 8),
                     static_cast<uint8_t>(value & 0xFF)};
  return this->write(data, 4) == i2c::ERROR_OK;
}

bool VL53L1XSensor::write_reg32_(uint16_t reg, uint32_t value) {
  uint8_t data[6] = {static_cast<uint8_t>(reg >> 8),    static_cast<uint8_t>(reg & 0xFF),
                     static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
                     static_cast<uint8_t>(value >> 8),  static_cast<uint8_t>(value & 0xFF)};
  return this->write(data, 6) == i2c::ERROR_OK;
}

bool VL53L1XSensor::read_reg_(uint16_t reg, uint8_t *value) {
  uint8_t reg_addr[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
  if (this->write(reg_addr, 2) != i2c::ERROR_OK) {
    return false;
  }
  return this->read(value, 1) == i2c::ERROR_OK;
}

bool VL53L1XSensor::read_reg16_(uint16_t reg, uint16_t *value) {
  uint8_t reg_addr[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
  if (this->write(reg_addr, 2) != i2c::ERROR_OK) {
    return false;
  }
  uint8_t buffer[2];
  if (this->read(buffer, 2) != i2c::ERROR_OK) {
    return false;
  }
  *value = (buffer[0] << 8) | buffer[1];
  return true;
}

// --- Timing Calculations ---

uint32_t VL53L1XSensor::calc_macro_period_(uint8_t vcsel_period) {
  // Macro period in microseconds
  // Formula from ST API
  uint32_t pll_period_us = (1 << 30) / this->fast_osc_frequency_;
  uint8_t vcsel_period_pclks = (vcsel_period + 1) << 1;
  return pll_period_us * vcsel_period_pclks * 2304 / 1000;
}

uint32_t VL53L1XSensor::timeout_mclks_to_microseconds_(uint32_t mclks, uint32_t macro_period_us) {
  return mclks * macro_period_us / 4096;
}

uint32_t VL53L1XSensor::timeout_microseconds_to_mclks_(uint32_t timeout_us, uint32_t macro_period_us) {
  return timeout_us * 4096 / macro_period_us;
}

uint16_t VL53L1XSensor::encode_timeout_(uint32_t mclks) {
  // Encode timeout as (mantissa, exponent) where timeout = mantissa * 2^exponent
  uint32_t mantissa;
  uint8_t exponent = 0;

  if (mclks > 0) {
    mantissa = mclks;
    while ((mantissa & 0xFFFFFF00) > 0) {
      mantissa >>= 1;
      exponent++;
    }
    return (exponent << 8) | (mantissa & 0xFF);
  }
  return 0;
}

uint32_t VL53L1XSensor::decode_timeout_(uint16_t encoded) {
  uint8_t mantissa = encoded & 0xFF;
  uint8_t exponent = (encoded >> 8) & 0xFF;
  return mantissa << exponent;
}

}  // namespace vl53l1x
}  // namespace esphome
