// VL53L1X time-of-flight distance sensor, ported from ST's VL53L1X Ultra Lite Driver (ULD).

#include "vl53l1x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <iterator>

namespace esphome::vl53l1x {

ESPHOME_LOG_TAG(TAG, "vl53l1x");

static constexpr uint8_t DEFAULT_ADDRESS = 0x29;
static constexpr uint16_t MODEL_ID = 0xEACC;

static constexpr uint16_t I2C_DEVICE_ADDRESS = 0x0001;
static constexpr uint16_t VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND = 0x0008;
static constexpr uint16_t VHV_CONFIG_INIT = 0x000B;
static constexpr uint16_t ALGO_PART_TO_PART_RANGE_OFFSET_MM = 0x001E;
static constexpr uint16_t MM_CONFIG_INNER_OFFSET_MM = 0x0020;
static constexpr uint16_t MM_CONFIG_OUTER_OFFSET_MM = 0x0022;
static constexpr uint16_t DEFAULT_CONFIG_START = 0x002D;
static constexpr uint16_t GPIO_TIO_HV_STATUS = 0x0031;
static constexpr uint16_t PHASECAL_CONFIG_TIMEOUT_MACROP = 0x004B;
static constexpr uint16_t RANGE_CONFIG_TIMEOUT_MACROP_A_HI = 0x005E;
static constexpr uint16_t RANGE_CONFIG_VCSEL_PERIOD_A = 0x0060;
static constexpr uint16_t RANGE_CONFIG_TIMEOUT_MACROP_B_HI = 0x0061;
static constexpr uint16_t RANGE_CONFIG_VCSEL_PERIOD_B = 0x0063;
static constexpr uint16_t RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS = 0x0066;
static constexpr uint16_t RANGE_CONFIG_VALID_PHASE_HIGH = 0x0069;
static constexpr uint16_t SD_CONFIG_WOI_SD0 = 0x0078;
static constexpr uint16_t SD_CONFIG_INITIAL_PHASE_SD0 = 0x007A;
static constexpr uint16_t ROI_CONFIG_USER_ROI_CENTRE_SPAD = 0x007F;
static constexpr uint16_t ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE = 0x0080;
static constexpr uint16_t SYSTEM_INTERRUPT_CLEAR = 0x0086;
static constexpr uint16_t SYSTEM_MODE_START = 0x0087;
static constexpr uint16_t RESULT_RANGE_STATUS = 0x0089;
static constexpr uint16_t FIRMWARE_SYSTEM_STATUS = 0x00E5;
static constexpr uint16_t IDENTIFICATION_MODEL_ID = 0x010F;
static constexpr uint16_t ROI_CONFIG_MODE_ROI_CENTRE_SPAD = 0x013E;

static constexpr uint8_t MODE_START_SINGLE_SHOT = 0x10;
// ROI centre for a ROI wider or taller than 10 SPADs (ULD SetROI)
static constexpr uint8_t ROI_CENTER_LARGE = 199;
// RESULT_RANGE_STATUS through RESULT_FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 (0x0096)
static constexpr size_t RESULT_LENGTH = 15;
// Raw range status that the ULD reports as a valid range
static constexpr uint8_t RANGE_STATUS_VALID = 9;

// ULD VL51L1X_DEFAULT_CONFIGURATION, registers 0x2D to 0x87
static const uint8_t DEFAULT_CONFIG[] PROGMEM = {
    0x00,  // 0x2D: reserved
    0x01,  // 0x2E: I2C pads, bit 0 set for pull-up at AVDD
    0x01,  // 0x2F: GPIO pads, bit 0 set for pull-up at AVDD
    0x01,  // 0x30: GPIO_HV_MUX_CTRL (bit 4 clear: data ready reads high)
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
    0x20,  // 0x46: CAL_CONFIG_SPAD_DC_TARGET_COUNT
    0x0B,  // 0x47: CAL_CONFIG_VCSEL_START
    0x00,  // 0x48: reserved
    0x00,  // 0x49: reserved
    0x02,  // 0x4A: reserved
    0x14,  // 0x4B: PHASECAL_CONFIG_TIMEOUT_MACROP
    0x21,  // 0x4C: reserved
    0x00,  // 0x4D: PHASECAL_CONFIG_OVERRIDE
    0x00,  // 0x4E: reserved
    0x05,  // 0x4F: DSS_CONFIG_ROI_MODE_CONTROL
    0x00,  // 0x50: SYSTEM_THRESH_RATE_HIGH
    0x00,  // 0x51: SYSTEM_THRESH_RATE_HIGH
    0x00,  // 0x52: SYSTEM_THRESH_RATE_LOW
    0x00,  // 0x53: SYSTEM_THRESH_RATE_LOW
    0xC8,  // 0x54: DSS_CONFIG_MANUAL_EFFECTIVE_SPADS_SELECT (200 << 8)
    0x00,  // 0x55: DSS_CONFIG_MANUAL_EFFECTIVE_SPADS_SELECT
    0x00,  // 0x56: reserved
    0x38,  // 0x57: DSS_CONFIG_APERTURE_ATTENUATION
    0xFF,  // 0x58: reserved
    0x01,  // 0x59: reserved
    0x00,  // 0x5A: reserved
    0x08,  // 0x5B: reserved
    0x00,  // 0x5C: reserved
    0x00,  // 0x5D: reserved
    0x01,  // 0x5E: RANGE_CONFIG_TIMEOUT_MACROP_A_HI
    0xCC,  // 0x5F: RANGE_CONFIG_TIMEOUT_MACROP_A_LO
    0x0F,  // 0x60: RANGE_CONFIG_VCSEL_PERIOD_A
    0x01,  // 0x61: RANGE_CONFIG_TIMEOUT_MACROP_B_HI
    0xF1,  // 0x62: RANGE_CONFIG_TIMEOUT_MACROP_B_LO
    0x0D,  // 0x63: RANGE_CONFIG_VCSEL_PERIOD_B
    0x01,  // 0x64: RANGE_CONFIG_SIGMA_THRESH
    0x68,  // 0x65: RANGE_CONFIG_SIGMA_THRESH (360)
    0x00,  // 0x66: RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS
    0x80,  // 0x67: RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS (1.0 MCPS)
    0x08,  // 0x68: reserved
    0xB8,  // 0x69: RANGE_CONFIG_VALID_PHASE_HIGH
    0x00,  // 0x6A: reserved
    0x00,  // 0x6B: reserved
    0x00,  // 0x6C: SYSTEM_INTERMEASUREMENT_PERIOD
    0x00,  // 0x6D: SYSTEM_INTERMEASUREMENT_PERIOD
    0x0F,  // 0x6E: SYSTEM_INTERMEASUREMENT_PERIOD
    0x89,  // 0x6F: SYSTEM_INTERMEASUREMENT_PERIOD
    0x00,  // 0x70: reserved
    0x00,  // 0x71: SYSTEM_THRESH_HIGH
    0x00,  // 0x72: SYSTEM_THRESH_HIGH
    0x00,  // 0x73: SYSTEM_THRESH_LOW
    0x00,  // 0x74: SYSTEM_THRESH_LOW
    0x00,  // 0x75: reserved
    0x00,  // 0x76: reserved
    0x01,  // 0x77: reserved
    0x0F,  // 0x78: SD_CONFIG_WOI_SD0
    0x0D,  // 0x79: SD_CONFIG_WOI_SD1
    0x0E,  // 0x7A: SD_CONFIG_INITIAL_PHASE_SD0
    0x0E,  // 0x7B: SD_CONFIG_INITIAL_PHASE_SD1
    0x00,  // 0x7C: reserved
    0x00,  // 0x7D: reserved
    0x02,  // 0x7E: reserved
    0xC7,  // 0x7F: ROI_CONFIG_USER_ROI_CENTRE_SPAD
    0xFF,  // 0x80: ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE
    0x9B,  // 0x81: SYSTEM_SEQUENCE_CONFIG
    0x00,  // 0x82: reserved
    0x00,  // 0x83: reserved
    0x00,  // 0x84: reserved
    0x01,  // 0x85: reserved
    0x00,  // 0x86: SYSTEM_INTERRUPT_CLEAR
    0x00,  // 0x87: SYSTEM_MODE_START
};

// ULD SetDistanceMode values, indexed by DistanceMode. The ULD only knows short and long; the
// medium row is the standard ranging preset of ST's full VL53L1 API.
struct DistanceModeConfig {
  uint8_t phasecal_timeout;
  uint8_t vcsel_period_a;
  uint8_t vcsel_period_b;
  uint8_t valid_phase_high;
  uint16_t woi_sd0;
  uint16_t initial_phase_sd0;
};
static const DistanceModeConfig DISTANCE_MODE_CONFIGS[] PROGMEM = {
    {0x14, 0x07, 0x05, 0x38, 0x0705, 0x0606},  // short
    {0x0B, 0x0B, 0x09, 0x78, 0x0B09, 0x0A0A},  // medium
    {0x0A, 0x0F, 0x0D, 0xB8, 0x0F0D, 0x0E0E},  // long
};

// ULD SetTimingBudgetInMs values
struct TimingBudgetConfig {
  uint16_t budget_ms;
  uint16_t macrop_a;
  uint16_t macrop_b;
};
static const TimingBudgetConfig SHORT_TIMING_BUDGETS[] PROGMEM = {
    {15, 0x001D, 0x0027},  {20, 0x0051, 0x006E},  {33, 0x00D6, 0x006E},  {50, 0x01AE, 0x01E8},
    {100, 0x02E1, 0x0388}, {200, 0x03E1, 0x0496}, {500, 0x0591, 0x05C1},
};
static const TimingBudgetConfig LONG_TIMING_BUDGETS[] PROGMEM = {
    {20, 0x001E, 0x0022},  {33, 0x0060, 0x006E},  {50, 0x00AD, 0x00C6},
    {100, 0x01CC, 0x01EA}, {200, 0x02D9, 0x02F8}, {500, 0x048F, 0x04A4},
};
// The ULD has no medium table: these are the long values scaled by the VCSEL period ratio
// (16/12 for A, 14/10 for B), since the macro period grows with the period and the timeouts
// count macro periods. The data-ready poll tolerates the small error this leaves.
static const TimingBudgetConfig MEDIUM_TIMING_BUDGETS[] PROGMEM = {
    {20, 0x0028, 0x0030},  {33, 0x0080, 0x009A},  {50, 0x00E7, 0x018B},
    {100, 0x0288, 0x02A4}, {200, 0x0390, 0x03AD}, {500, 0x04BE, 0x04E5},
};

// Copies the entry for budget_ms out of flash; false when the mode has no such budget.
static bool find_timing_budget(DistanceMode mode, uint16_t budget_ms, TimingBudgetConfig *out) {
  const TimingBudgetConfig *table = LONG_TIMING_BUDGETS;
  size_t count = std::size(LONG_TIMING_BUDGETS);
  if (mode == DISTANCE_MODE_SHORT) {
    table = SHORT_TIMING_BUDGETS;
    count = std::size(SHORT_TIMING_BUDGETS);
  } else if (mode == DISTANCE_MODE_MEDIUM) {
    table = MEDIUM_TIMING_BUDGETS;
    count = std::size(MEDIUM_TIMING_BUDGETS);
  }
  for (size_t i = 0; i < count; i++) {
    progmem_memcpy(out, &table[i], sizeof(TimingBudgetConfig));
    if (out->budget_ms == budget_ms)
      return true;
  }
  return false;
}

VL53L1XSensor *VL53L1XSensor::first_sensor = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

VL53L1XSensor::VL53L1XSensor() {
  this->next_ = VL53L1XSensor::first_sensor;
  VL53L1XSensor::first_sensor = this;
}

void VL53L1XSensor::setup() {
  // The first sensor to set up holds every sensor with an enable pin in reset, so each one
  // comes back at the default address only when its own setup() releases it.
  if (VL53L1XSensor::first_sensor != nullptr) {
    for (auto *other = VL53L1XSensor::first_sensor; other != nullptr; other = other->next_) {
      if (other->enable_pin_ != nullptr) {
        other->enable_pin_->setup();
        other->enable_pin_->digital_write(false);
      }
    }
    VL53L1XSensor::first_sensor = nullptr;
  }

  const uint8_t final_address = this->address_;
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
    delay(2);
  }
  this->set_i2c_address(DEFAULT_ADDRESS);

  if (!this->wait_for_boot_()) {
    this->fail_at_default_address_(LOG_STR("Boot timeout"));
    return;
  }

  uint16_t model_id;
  if (!this->read_word16_(IDENTIFICATION_MODEL_ID, &model_id)) {
    this->fail_at_default_address_(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (model_id != MODEL_ID) {
    ESP_LOGE(TAG, "Wrong model ID 0x%04X", model_id);
    this->fail_at_default_address_(LOG_STR("Wrong model ID"));
    return;
  }

  if (final_address != DEFAULT_ADDRESS) {
    if (!this->write_byte16_(I2C_DEVICE_ADDRESS, final_address & 0x7F)) {
      this->fail_at_default_address_(LOG_STR("Setting address failed"));
      return;
    }
    this->set_i2c_address(final_address);
  }

  if (!this->configure_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // The ULD ranges once during init to calibrate; loop() finishes that from the first result.
  if (!this->start_ranging_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->measurement_start_ = millis();
}

// A sensor that failed before it was re-addressed is still answering at 0x29, which would
// collide with the next sibling's bring-up, so hold it in reset.
void VL53L1XSensor::fail_at_default_address_(const LogString *message) {
  if (this->enable_pin_ != nullptr)
    this->enable_pin_->digital_write(false);
  this->mark_failed(message);
}

bool VL53L1XSensor::wait_for_boot_() {
  for (uint8_t i = 0; i < 100; i++) {
    uint8_t status;
    if (this->read_byte16_(FIRMWARE_SYSTEM_STATUS, &status) && (status & 0x01))
      return true;
    delay(1);
  }
  return false;
}

bool VL53L1XSensor::configure_() {
  TimingBudgetConfig budget;
  if (!find_timing_budget(this->distance_mode_, this->budget_ms_, &budget)) {
    ESP_LOGE(TAG, "Unsupported timing budget %u ms", this->budget_ms_);
    return false;
  }
  DistanceModeConfig mode;
  progmem_memcpy(&mode, &DISTANCE_MODE_CONFIGS[this->distance_mode_], sizeof(mode));

  uint8_t config[sizeof(DEFAULT_CONFIG)];
  progmem_memcpy(config, DEFAULT_CONFIG, sizeof(config));
  // ULD SetROI: a ROI of at most 10x10 is centred on the part's calibrated optical centre.
  uint8_t center = this->roi_center_;
  if (center == 0) {
    center = ROI_CENTER_LARGE;
    if (this->roi_width_ <= 10 && this->roi_height_ <= 10 &&
        !this->read_byte16_(ROI_CONFIG_MODE_ROI_CENTRE_SPAD, &center)) {
      return false;
    }
  }
  // The register address auto-increments, so the whole table goes in one write.
  return this->write_register16(DEFAULT_CONFIG_START, config, sizeof(config)) == i2c::ERROR_OK &&
         // SetDistanceMode
         this->write_byte16_(PHASECAL_CONFIG_TIMEOUT_MACROP, mode.phasecal_timeout) &&
         this->write_byte16_(RANGE_CONFIG_VCSEL_PERIOD_A, mode.vcsel_period_a) &&
         this->write_byte16_(RANGE_CONFIG_VCSEL_PERIOD_B, mode.vcsel_period_b) &&
         this->write_byte16_(RANGE_CONFIG_VALID_PHASE_HIGH, mode.valid_phase_high) &&
         this->write_word16_(SD_CONFIG_WOI_SD0, mode.woi_sd0) &&
         this->write_word16_(SD_CONFIG_INITIAL_PHASE_SD0, mode.initial_phase_sd0) &&
         // SetTimingBudgetInMs
         this->write_word16_(RANGE_CONFIG_TIMEOUT_MACROP_A_HI, budget.macrop_a) &&
         this->write_word16_(RANGE_CONFIG_TIMEOUT_MACROP_B_HI, budget.macrop_b) &&
         // SetOffset
         this->write_word16_(ALGO_PART_TO_PART_RANGE_OFFSET_MM, static_cast<uint16_t>(this->offset_mm_ * 4)) &&
         this->write_word16_(MM_CONFIG_INNER_OFFSET_MM, 0) && this->write_word16_(MM_CONFIG_OUTER_OFFSET_MM, 0) &&
         // SetROI and SetROICenter
         this->write_byte16_(ROI_CONFIG_USER_ROI_CENTRE_SPAD, center) &&
         this->write_byte16_(ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE,
                             static_cast<uint8_t>(((this->roi_height_ - 1) << 4) | (this->roi_width_ - 1))) &&
         // SetSignalThreshold
         this->write_word16_(RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS, this->signal_rate_limit_);
}

bool VL53L1XSensor::start_ranging_() { return this->write_byte16_(SYSTEM_MODE_START, MODE_START_SINGLE_SHOT); }

void VL53L1XSensor::update() {
  if (this->is_in_loop_state()) {
    ESP_LOGD(TAG, "Measurement still running");
    return;
  }
  if (!this->start_ranging_()) {
    ESP_LOGW(TAG, "Starting measurement failed");
    this->status_set_warning();
    this->publish_state(NAN);
    return;
  }
  this->measurement_start_ = App.get_loop_component_start_time();
  this->enable_loop();
}

void VL53L1XSensor::loop() {
  const uint32_t elapsed = App.get_loop_component_start_time() - this->measurement_start_;
  if (elapsed < this->budget_ms_)
    return;

  uint8_t gpio_status;
  if (!this->read_byte16_(GPIO_TIO_HV_STATUS, &gpio_status) || (gpio_status & 0x01) == 0) {
    if (elapsed > static_cast<uint32_t>(this->budget_ms_) * 2 + 100) {
      ESP_LOGW(TAG, "Measurement timed out");
      this->status_set_warning();
      this->publish_state(NAN);
      this->disable_loop();
    }
    return;
  }

  uint8_t result[RESULT_LENGTH];
  if (this->read_register16(RESULT_RANGE_STATUS, result, sizeof(result)) != i2c::ERROR_OK ||
      !this->write_byte16_(SYSTEM_INTERRUPT_CLEAR, 0x01)) {
    ESP_LOGW(TAG, "Reading result failed");
    this->status_set_warning();
    this->publish_state(NAN);
    this->disable_loop();
    return;
  }
  this->disable_loop();

  if (this->first_result_) {
    // ULD SensorInit: after the first ranging, start VHV calibration from the previous temperature.
    if (!this->write_byte16_(VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x09) || !this->write_byte16_(VHV_CONFIG_INIT, 0)) {
      this->status_set_warning();
      return;
    }
    this->first_result_ = false;
    return;
  }

  this->status_clear_warning();
  const uint8_t range_status = result[0] & 0x1F;
  if (range_status != RANGE_STATUS_VALID) {
    ESP_LOGD(TAG, "Range status %u", range_status);
    this->publish_state(NAN);
    return;
  }
  const uint16_t distance_mm = encode_uint16(result[13], result[14]);
  this->publish_state(distance_mm / 1000.0f);
}

static const LogString *distance_mode_name(DistanceMode mode) {
  switch (mode) {
    case DISTANCE_MODE_SHORT:
      return LOG_STR("short");
    case DISTANCE_MODE_MEDIUM:
      return LOG_STR("medium");
    default:
      return LOG_STR("long");
  }
}

void VL53L1XSensor::dump_config() {
  LOG_SENSOR("", "VL53L1X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  ESP_LOGCONFIG(TAG,
                "  Distance Mode: %s\n"
                "  Timing Budget: %u ms\n"
                "  ROI: %ux%u\n"
                "  Offset: %d mm",
                LOG_STR_ARG(distance_mode_name(this->distance_mode_)), this->budget_ms_, this->roi_width_,
                this->roi_height_, this->offset_mm_);
  if (this->roi_center_ != 0) {
    ESP_LOGCONFIG(TAG, "  ROI Center: %u", this->roi_center_);
  }
}

bool VL53L1XSensor::write_byte16_(uint16_t reg, uint8_t value) {
  return this->write_register16(reg, &value, 1) == i2c::ERROR_OK;
}

bool VL53L1XSensor::write_word16_(uint16_t reg, uint16_t value) {
  const uint8_t data[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  return this->write_register16(reg, data, 2) == i2c::ERROR_OK;
}

bool VL53L1XSensor::read_byte16_(uint16_t reg, uint8_t *value) {
  return this->read_register16(reg, value, 1) == i2c::ERROR_OK;
}

bool VL53L1XSensor::read_word16_(uint16_t reg, uint16_t *value) {
  uint8_t data[2];
  if (this->read_register16(reg, data, 2) != i2c::ERROR_OK)
    return false;
  *value = encode_uint16(data[0], data[1]);
  return true;
}

}  // namespace esphome::vl53l1x
