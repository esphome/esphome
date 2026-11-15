#include "vl6180x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/entity_base.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <algorithm>

namespace esphome::vl6180x {

ESPHOME_LOG_TAG(TAG, "vl6180x");
static constexpr uint32_t LUX_INTERVAL_ID = 0;
static constexpr uint32_t ALS_INTEGRATION_MARGIN_MS = 400;

PROGMEM_STRING_TABLE(RangeStatusStrings, "OK", "Hardware Fault", "Target Too Close", "No Target", "Out of Range",
                     "Sensor Error", "Timeout", "Recovering", "Failed");
PROGMEM_STRING_TABLE(RangeErrorStrings, "None", "VCSEL continuity", "VCSEL watchdog test", "VCSEL watchdog",
                     "PLL1 lock", "PLL2 lock", "early convergence", "max convergence", "no target (ignored)", "unknown",
                     "unknown", "no target (SNR)", "raw underflow", "raw overflow", "algo underflow", "algo overflow");

// ---------------------------------------------------------------------------
// Register map
// ---------------------------------------------------------------------------
static constexpr uint16_t REG_IDENTIFICATION_MODEL_ID = 0x000;
static constexpr uint16_t REG_SYSTEM_MODE_GPIO1 = 0x011;
static constexpr uint16_t REG_SYSTEM_INTERRUPT_CONFIG = 0x014;
static constexpr uint16_t REG_SYSTEM_INTERRUPT_CLEAR = 0x015;
static constexpr uint16_t REG_SYSTEM_FRESH_OUT_OF_RESET = 0x016;
static constexpr uint16_t REG_SYSRANGE_START = 0x018;
static constexpr uint16_t REG_SYSALS_START = 0x038;
static constexpr uint16_t REG_SYSALS_ANALOGUE_GAIN = 0x03F;
static constexpr uint16_t REG_SYSALS_INTEGRATION_PERIOD_HI = 0x040;
static constexpr uint16_t REG_SYSALS_INTEGRATION_PERIOD_LO = 0x041;
static constexpr uint16_t REG_SYSRANGE_VHV_RECALIBRATE = 0x02E;
static constexpr uint16_t REG_SYSRANGE_VHV_REPEAT_RATE = 0x031;
static constexpr uint16_t REG_RESULT_ALS_VAL = 0x050;
static constexpr uint16_t REG_RESULT_RANGE_VAL = 0x062;
static constexpr uint16_t REG_RESULT_ALS_STATUS = 0x04E;
static constexpr uint8_t ALS_ERROR_MASK = 0xF0;
static constexpr uint16_t REG_RESULT_RANGE_STATUS = 0x04D;
static constexpr uint16_t REG_RESULT_INTERRUPT_STATUS_GPIO = 0x04F;
static constexpr uint16_t REG_SYSRANGE_CROSSTALK_VALID_HEIGHT = 0x021;
static constexpr uint16_t REG_SYSRANGE_PART_TO_PART_OFFSET = 0x024;
static constexpr uint16_t REG_SYSRANGE_RANGE_CHECK_ENABLES = 0x02D;
static constexpr uint16_t REG_RANGE_SCALING = 0x096;
static constexpr uint16_t REG_I2C_DEVICE_ADDRESS = 0x212;
static constexpr uint16_t RANGE_SCALING_VALUES[] PROGMEM = {0, 253, 127, 84};

// Measurement timeouts (ms)
static constexpr uint32_t RANGE_READY_TIMEOUT_MS = 250;
static constexpr uint32_t RANGE_RESULT_TIMEOUT_MS = 200;
static constexpr uint32_t ALS_READY_TIMEOUT_MS = 100;
static constexpr uint32_t RECOVERY_COORDINATION_DELAY_MS = 100;

// ---------------------------------------------------------------------------
// Low-level I2C — 16-bit register addressing, MSB first
// ---------------------------------------------------------------------------

bool VL6180XComponent::write_reg8_(uint16_t reg, uint8_t value) {
  uint8_t buf[3] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF), value};
  i2c::ErrorCode err = this->write(buf, 3);
  const bool ok = this->status_comm_check(err == i2c::ERROR_OK);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] write_reg8 0x%04X=0x%02X err=%d", this->name_str_, reg, value, (int) err);
  }
  return ok;
}

bool VL6180XComponent::write_reg16_(uint16_t reg, uint16_t value) {
  uint8_t buf[4] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF), (uint8_t) (value >> 8), (uint8_t) (value & 0xFF)};
  i2c::ErrorCode err = this->write(buf, 4);
  const bool ok = this->status_comm_check(err == i2c::ERROR_OK);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] write_reg16 0x%04X=0x%04X err=%d", this->name_str_, reg, value, (int) err);
  }
  return ok;
}

bool VL6180XComponent::read_reg8_(uint16_t reg, uint8_t &value) {
  uint8_t addr[2] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF)};
  i2c::ErrorCode err = this->write_read(addr, 2, &value, 1);
  const bool ok = this->status_comm_check(err == i2c::ERROR_OK);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] read_reg8 0x%04X err=%d", this->name_str_, reg, (int) err);
  }
  return ok;
}

bool VL6180XComponent::read_reg16_(uint16_t reg, uint16_t &value) {
  uint8_t addr[2] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF)};
  uint8_t data[2];
  i2c::ErrorCode err = this->write_read(addr, 2, data, 2);
  const bool ok = this->status_comm_check(err == i2c::ERROR_OK);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] read_reg16 0x%04X err=%d", this->name_str_, reg, (int) err);
    return false;
  }
  value = ((uint16_t) data[0] << 8) | data[1];
  return true;
}

// ---------------------------------------------------------------------------
// Address assignment
// ---------------------------------------------------------------------------

bool VL6180XComponent::assign_address_(uint8_t new_addr) {
  if (!this->write_reg8_(REG_I2C_DEVICE_ADDRESS, new_addr))
    return false;
  this->set_i2c_address(new_addr);
  ESP_LOGD(TAG, "[%s] I2C address assigned to 0x%02X", this->name_str_, new_addr);
  return true;
}

// ---------------------------------------------------------------------------
// Mandatory private settings — AN4545 Table 7
// ---------------------------------------------------------------------------

bool VL6180XComponent::load_mandatory_settings_() {
  uint8_t fresh = 0;
  if (!this->read_reg8_(REG_SYSTEM_FRESH_OUT_OF_RESET, fresh)) {
    ESP_LOGE(TAG, "[%s] Failed to read FRESH_OUT_OF_RESET", this->name_str_);
    return false;
  }
  ESP_LOGD(TAG, "[%s] FRESH_OUT_OF_RESET=0x%02X", this->name_str_, fresh);
  uint8_t offset = 0;
  if (!this->read_reg8_(REG_SYSRANGE_PART_TO_PART_OFFSET, offset))
    return false;
  this->ptp_offset_ = static_cast<int8_t>(offset);
  if ((fresh & 0x01) == 0) {
    // The offset register is already scaled when the sensor survived an MCU
    // reboot. Restore its 1x value before applying the configured scaler.
    uint16_t current_scaling = 0;
    if (!this->read_reg16_(REG_RANGE_SCALING, current_scaling))
      return false;
    uint16_t scale_2 = 0;
    uint16_t scale_3 = 0;
    progmem_memcpy(&scale_2, &RANGE_SCALING_VALUES[2], sizeof(scale_2));
    progmem_memcpy(&scale_3, &RANGE_SCALING_VALUES[3], sizeof(scale_3));
    uint8_t detected_scaling = current_scaling == scale_3 ? 3 : current_scaling == scale_2 ? 2 : 1;
    this->ptp_offset_ = static_cast<int8_t>(this->ptp_offset_ * detected_scaling);
    // ST's mandatory private register table is a one-time initialization and
    // must only be written after a hardware reset.
    ESP_LOGD(TAG, "[%s] Sensor is not fresh out of reset; skipping one-time settings", this->name_str_);
    return true;
  }

  struct RegisterValue {
    uint16_t reg;
    uint8_t value;
  };
  static constexpr RegisterValue INITIAL_SETTINGS[] PROGMEM = {
      {0x0207, 0x01}, {0x0208, 0x01}, {0x0096, 0x00}, {0x0097, 0xFD}, {0x00E3, 0x00}, {0x00E4, 0x04},
      {0x00E5, 0x02}, {0x00E6, 0x01}, {0x00E7, 0x03}, {0x00F5, 0x02}, {0x00D9, 0x05}, {0x00DB, 0xCE},
      {0x00DC, 0x03}, {0x00DD, 0xF8}, {0x009F, 0x00}, {0x00A3, 0x3C}, {0x00B7, 0x00}, {0x00BB, 0x3C},
      {0x00B2, 0x09}, {0x00CA, 0x09}, {0x0198, 0x01}, {0x01B0, 0x17}, {0x01AD, 0x00}, {0x00FF, 0x05},
      {0x0100, 0x05}, {0x0199, 0x05}, {0x01A6, 0x1B}, {0x01AC, 0x3E}, {0x01A7, 0x1F}, {0x0030, 0x00},
  };
  for (const auto &entry : INITIAL_SETTINGS) {
    RegisterValue setting;
    progmem_memcpy(&setting, &entry, sizeof(setting));
    if (!this->write_reg8_(setting.reg, setting.value)) {
      ESP_LOGE(TAG, "[%s] Failed write 0x%04X", this->name_str_, setting.reg);
      return false;
    }
  }

  if (!this->write_reg8_(REG_SYSTEM_FRESH_OUT_OF_RESET, 0x00))
    return false;
  ESP_LOGD(TAG, "[%s] Mandatory registers done", this->name_str_);
  return true;
}

// ---------------------------------------------------------------------------
// Default configuration
// ---------------------------------------------------------------------------

bool VL6180XComponent::configure_defaults_() {
  if (!this->write_reg8_(0x010A, 0x30))
    return false;  // averaging sample period
  if (!this->write_reg8_(REG_SYSALS_ANALOGUE_GAIN, 0x40 | this->als_gain_))
    return false;  // ALS gain 1.0x default
  if (!this->write_reg8_(REG_SYSRANGE_VHV_REPEAT_RATE, 0xFF))
    return false;  // Auto VHV temperature recalibration every 255 range measurements.
  if (!this->write_reg8_(REG_SYSRANGE_VHV_RECALIBRATE, 0x01))
    return false;  // Run the initial VHV calibration while both measurement engines are stopped.
  const uint16_t integration_period = this->als_integration_time_ms_ - 1;
  if (!this->write_reg8_(REG_SYSALS_INTEGRATION_PERIOD_HI, integration_period >> 8))
    return false;
  if (!this->write_reg8_(REG_SYSALS_INTEGRATION_PERIOD_LO, integration_period & 0xFF))
    return false;
  if (!this->write_reg8_(0x001C, 0x32))
    return false;  // max convergence time 50ms
  if (!this->write_reg8_(0x002D, 0x10))
    return false;  // range check enables: SNR only (not 0xFF)
  // GPIO1 is optional. Configure active-low new-sample interrupts only when
  // an MCU interrupt input is supplied; otherwise leave the sensor pin Hi-Z.
  if (!this->write_reg8_(REG_SYSTEM_MODE_GPIO1, this->interrupt_pin_ != nullptr ? 0x10 : 0x20))
    return false;
  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CONFIG, 0x00))
    return false;
  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x07))
    return false;
  return this->apply_scaling_();
}

bool VL6180XComponent::apply_scaling_() {
  const uint8_t scale = this->scaling_;
  if (scale < 1 || scale > 3)
    return false;
  uint16_t scale_value = 0;
  progmem_memcpy(&scale_value, &RANGE_SCALING_VALUES[scale], sizeof(scale_value));
  if (!this->write_reg16_(REG_RANGE_SCALING, scale_value))
    return false;
  if (!this->write_reg8_(REG_SYSRANGE_PART_TO_PART_OFFSET, static_cast<uint8_t>(this->ptp_offset_ / scale)))
    return false;
  if (!this->write_reg8_(REG_SYSRANGE_CROSSTALK_VALID_HEIGHT, 20 / scale))
    return false;

  uint8_t range_check = 0;
  if (!this->read_reg8_(REG_SYSRANGE_RANGE_CHECK_ENABLES, range_check))
    return false;
  range_check = (range_check & 0xFE) | (scale == 1 ? 1 : 0);
  if (!this->write_reg8_(REG_SYSRANGE_RANGE_CHECK_ENABLES, range_check))
    return false;
  ESP_LOGD(TAG, "[%s] Range scaler set to %ux", this->name_str_, static_cast<unsigned>(scale));
  return true;
}

// ---------------------------------------------------------------------------
// Sensor init
// ---------------------------------------------------------------------------

bool VL6180XComponent::init_sensor_() {
  uint8_t model_id = 0;
  if (!this->read_reg8_(REG_IDENTIFICATION_MODEL_ID, model_id)) {
    ESP_LOGE(TAG, "[%s] Failed to read model ID", this->name_str_);
    return false;
  }
  if (model_id != 0xB4) {
    ESP_LOGE(TAG, "[%s] Wrong model ID 0x%02X (expected 0xB4)", this->name_str_, model_id);
    return false;
  }
  ESP_LOGD(TAG, "[%s] Model ID 0xB4 OK", this->name_str_);
  uint8_t start_mode = 0, range_status = 0;
  if (this->read_reg8_(REG_SYSRANGE_START, start_mode) && this->read_reg8_(REG_RESULT_RANGE_STATUS, range_status)) {
    ESP_LOGI(TAG, "[%s] pre-init SYSRANGE_START=0x%02X RANGE_STATUS=0x%02X", this->name_str_, start_mode, range_status);
  }
  if (!this->load_mandatory_settings_())
    return false;
  if (!this->configure_defaults_())
    return false;
  return true;
}

// ---------------------------------------------------------------------------
// run_init_sequence (static)
//
// Full Option-1 reinitialisation of every sensor. Called by both setup() and
// the recovery path. Resets all instance state machines to IDLE before
// touching the hardware so no stale state is left after completion.
// ---------------------------------------------------------------------------

void VL6180XComponent::run_init_sequence(i2c::I2CBus *bus) {
  auto &all = all_instances();

  // Reset all state machines — any in-progress measurement is abandoned.
  // Publish NAN / "Recovering" so HA reflects the outage immediately.
  for (auto *inst : all) {
    if (inst->bus_ != bus)
      continue;
    if (inst->is_failed())
      continue;
    if (inst->state_ != MeasurementState::MEASUREMENT_STATE_IDLE &&
        inst->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT) {
      inst->publish_range_error_(RangeStatus::RANGE_STATUS_RECOVERING);
    }
    inst->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
    inst->lux_pending_ = false;
    inst->range_pending_ = false;
  }

  // Step 1: all XSHUT pins low
  for (auto *inst : all) {
    if (inst->bus_ != bus)
      continue;
    if (inst->enable_pin_ != nullptr)
      inst->enable_pin_->digital_write(false);
  }
  delay(10);

  // Step 2: initialize non-default addresses first, then 0x29 sensors. Keeping
  // the registry in place avoids stable_sort's temporary allocation during
  // recovery, which can run after setup on a long-lived device.
  for (uint8_t phase = 0; phase < 2; phase++) {
    for (auto *inst : all) {
      if (inst->bus_ != bus)
        continue;
      const uint8_t target_addr = inst->get_i2c_address();
      const bool is_default_address = target_addr == 0x29;
      if (is_default_address != (phase == 1))
        continue;

      if (inst->enable_pin_ != nullptr) {
        inst->enable_pin_->digital_write(true);
        delay(10);
      }

      if (target_addr != 0x29)
        inst->set_i2c_address(0x29);

      ESP_LOGI(TAG, "[%s] (Re)initialising at 0x29 (target: 0x%02X)", inst->name_str_, target_addr);

      bool initialized_at_default = inst->init_sensor_();
      bool init_ok = initialized_at_default;
      if (!init_ok && target_addr != 0x29) {
        // Some modules retain the programmed I2C address across MCU/XSHUT
        // resets. If the default address is absent, retry at the configured
        // target before declaring initialization failed.
        ESP_LOGW(TAG, "[%s] No response at 0x29; retrying configured address 0x%02X", inst->name_str_, target_addr);
        inst->set_i2c_address(target_addr);
        init_ok = inst->init_sensor_();
      }

      if (!init_ok) {
        ESP_LOGE(TAG, "[%s] Init failed", inst->name_str_);
        // Isolate a sensor that remained at the default 0x29 address so it
        // cannot collide with the next sensor released from XSHUT.
        if (inst->enable_pin_ != nullptr)
          inst->enable_pin_->digital_write(false);
        inst->set_i2c_address(target_addr);
        // Don't mark_failed() here — the recovery loop handles retries.
        // Leave initialized_ false so the instance stays in RECOVERY_WAIT.
        inst->initialized_ = false;
        inst->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
        continue;
      }

      if (initialized_at_default && !inst->assign_address_(target_addr)) {
        ESP_LOGE(TAG, "[%s] Address assignment to 0x%02X failed", inst->name_str_, target_addr);
        // Keep a failed assignment at 0x29 isolated while the remaining sensors
        // are initialized. Recovery will retry the complete XSHUT sequence.
        if (inst->enable_pin_ != nullptr)
          inst->enable_pin_->digital_write(false);
        inst->set_i2c_address(target_addr);
        inst->initialized_ = false;
        inst->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
        continue;
      }
      if (!initialized_at_default) {
        ESP_LOGD(TAG, "[%s] Sensor already responding at configured address 0x%02X", inst->name_str_, target_addr);
      }

      inst->initialized_ = true;
      inst->consecutive_failures_ = 0;
      inst->status_clear_warning();
      // Leave state_ as IDLE — normal measurement will resume on next update()/lux timer
      ESP_LOGI(TAG, "[%s] Ready at 0x%02X", inst->name_str_, target_addr);
    }
  }
}

// ---------------------------------------------------------------------------
// Health monitoring helpers
// ---------------------------------------------------------------------------

void VL6180XComponent::record_failure_() {
  this->consecutive_failures_++;
  this->status_set_warning();
  if (this->consecutive_failures_ >= this->failure_threshold_ &&
      this->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT) {
    ESP_LOGW(TAG, "[%s] %d consecutive failures — entering recovery", this->name_str_, this->consecutive_failures_);
    this->reinit_attempts_ = 0;
    this->reinit_due_at_ = this->now_() + this->retry_interval_ms_;
    this->initialized_ = false;
    this->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
    // Do not call publish_range_error_() here: it calls record_failure_(),
    // which would recurse until the ESP32 loop task overflows its stack.
    PUBLISH_STATE(this->range_sensor_, NAN);
    PUBLISH_STATE(this->range_status_text_sensor_,
                  RangeStatusStrings::get_progmem_str(static_cast<uint8_t>(RangeStatus::RANGE_STATUS_RECOVERING),
                                                      RangeStatusStrings::LAST_INDEX));
    PUBLISH_STATE(this->lux_sensor_, NAN);
  }
}

void VL6180XComponent::record_success_() {
  this->consecutive_failures_ = 0;
  this->status_clear_warning();
}

void VL6180XComponent::fail_range_measurement_(RangeStatus status) {
  this->publish_range_error_(status);
  if (this->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
}

void VL6180XComponent::fail_lux_measurement_() {
  PUBLISH_STATE(this->lux_sensor_, NAN);
  this->record_failure_();
  if (this->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
}

// ---------------------------------------------------------------------------
// Publish helpers
// ---------------------------------------------------------------------------

void VL6180XComponent::publish_range_(uint8_t raw, uint8_t error_code) {
  // Sensor-level errors (not "no target" / "out of range") count as failures
  // for health monitoring. "No Target", "Target Too Close", "Out of Range"
  // are valid measurements — the sensor is working, just reporting conditions.
  // Hardware Fault counts as a sensor failure.
  if (error_code != 0) {
    const uint8_t error_index = error_code <= 15 ? error_code : 9;
    const bool is_sensor_failure = error_code <= 5 || error_code == 9 || error_code == 10 || error_code > 15;
    RangeStatus status = RangeStatus::RANGE_STATUS_SENSOR_ERROR;
    if (error_code >= 1 && error_code <= 5) {
      status = RangeStatus::RANGE_STATUS_HARDWARE_FAULT;
    } else if (error_code == 12 || error_code == 14) {
      status = RangeStatus::RANGE_STATUS_TARGET_TOO_CLOSE;
    } else if (error_code == 6 || error_code == 7 || error_code == 8 || error_code == 11) {
      status = RangeStatus::RANGE_STATUS_NO_TARGET;
    } else if (error_code == 13 || error_code == 15) {
      status = RangeStatus::RANGE_STATUS_OUT_OF_RANGE;
    }
    ESP_LOGW(TAG, "[%s] Range error %d: %s (raw=%d)", this->name_str_, error_code,
             LOG_STR_ARG(RangeErrorStrings::get_log_str(error_index, RangeErrorStrings::LAST_INDEX)), raw);
    PUBLISH_STATE(this->range_sensor_, NAN);
    PUBLISH_STATE(this->range_status_text_sensor_,
                  RangeStatusStrings::get_progmem_str(static_cast<uint8_t>(status), RangeStatusStrings::LAST_INDEX));

    if (is_sensor_failure) {
      this->record_failure_();
    } else {
      this->record_success_();
    }
    return;
  }

  const float distance_mm = static_cast<float>(raw) * this->scaling_;
  ESP_LOGD(TAG, "[%s] Range: %.0f mm (raw=%u, scaler=%ux)", this->name_str_, distance_mm, static_cast<unsigned>(raw),
           static_cast<unsigned>(this->scaling_));
  PUBLISH_STATE(this->range_sensor_, distance_mm);
  PUBLISH_STATE(this->range_status_text_sensor_,
                RangeStatusStrings::get_progmem_str(static_cast<uint8_t>(RangeStatus::RANGE_STATUS_OK),
                                                    RangeStatusStrings::LAST_INDEX));
  this->record_success_();
}

void VL6180XComponent::publish_range_error_(RangeStatus status) {
  // Called for I2C-level errors and timeouts — always a sensor failure
  PUBLISH_STATE(this->range_sensor_, NAN);
  PUBLISH_STATE(this->range_status_text_sensor_,
                RangeStatusStrings::get_progmem_str(static_cast<uint8_t>(status), RangeStatusStrings::LAST_INDEX));
  this->record_failure_();
}

void VL6180XComponent::publish_lux_(uint16_t raw) {
  const float lux = 0.32f * static_cast<float>(raw) * 100.0f /
                    (this->als_gain_factor_ * static_cast<float>(this->als_integration_time_ms_));
  ESP_LOGD(TAG, "[%s] Lux raw=%u → %.2f lx", this->name_str_, static_cast<unsigned>(raw), lux);
  PUBLISH_STATE(this->lux_sensor_, lux);
  this->record_success_();
}

// ---------------------------------------------------------------------------
// State machine — range start
// ---------------------------------------------------------------------------

void VL6180XComponent::do_range_start_() {
  if (this->recovery_pause_) {
    this->range_pending_ = false;
    this->lux_pending_ = false;
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
    return;
  }

  uint8_t icfg = 0;
  if (!this->read_reg8_(REG_SYSTEM_INTERRUPT_CONFIG, icfg)) {
    this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
    return;
  }
  icfg = (icfg & ~0x07) | 0x04;
  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CONFIG, icfg)) {
    this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
    return;
  }

  uint8_t status = 0;
  if (!this->read_reg8_(REG_RESULT_RANGE_STATUS, status)) {
    this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
    return;
  }
  if (!(status & 0x01)) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      ESP_LOGW(TAG, "[%s] Range: device not ready (0x%02X)", this->name_str_, status);
      this->fail_range_measurement_(RangeStatus::RANGE_STATUS_TIMEOUT);
    }
    return;
  }

  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x07)) {
    this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
    return;
  }
  if (!this->write_reg8_(REG_SYSRANGE_START, 0x01)) {
    this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
    return;
  }

  this->state_deadline_ = this->now_() + RANGE_RESULT_TIMEOUT_MS;
  this->state_ = MeasurementState::MEASUREMENT_STATE_RANGE_WAIT;
}

// ---------------------------------------------------------------------------
// State machine — range wait
// ---------------------------------------------------------------------------

void VL6180XComponent::do_range_wait_() {
  // With GPIO1 connected, avoid repeated I2C status polling until the sensor
  // signals new data. The register is still checked after the edge as a guard.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      uint8_t irq = 0;
      if (!this->read_reg8_(REG_RESULT_INTERRUPT_STATUS_GPIO, irq)) {
        this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
        return;
      }
      ESP_LOGW(TAG, "[%s] Range: interrupt timeout (irq=0x%02X)", this->name_str_, irq);
      this->fail_range_measurement_(RangeStatus::RANGE_STATUS_TIMEOUT);
    }
    return;
  }

  uint8_t irq = 0;
  if (!this->read_reg8_(REG_RESULT_INTERRUPT_STATUS_GPIO, irq)) {
    this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
    return;
  }

  if (!(irq & 0x04)) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      ESP_LOGW(TAG, "[%s] Range: timeout (irq=0x%02X)", this->name_str_, irq);
      this->fail_range_measurement_(RangeStatus::RANGE_STATUS_TIMEOUT);
    }
    return;
  }

  uint8_t raw = 0;
  uint8_t range_status = 0;
  if (!this->read_reg8_(REG_RESULT_RANGE_VAL, raw) || !this->read_reg8_(REG_RESULT_RANGE_STATUS, range_status) ||
      !this->write_reg8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x07)) {
    this->fail_range_measurement_(RangeStatus::RANGE_STATUS_SENSOR_ERROR);
    return;
  }

  // Note: publish_range_() calls record_success_() or record_failure_() as
  // appropriate — state_ must still be RANGE_WAIT at that point so that
  // record_failure_() → RECOVERY_WAIT transition is valid. We set state_
  // after publish_range_() returns.
  this->publish_range_(raw, (range_status >> 4) & 0x0F);

  // If we just entered RECOVERY_WAIT don't overwrite that state
  if (this->state_ == MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    return;

  if (this->recovery_pause_) {
    this->range_pending_ = false;
    this->lux_pending_ = false;
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
    return;
  }

  if (this->range_pending_) {
    this->range_pending_ = false;
    this->state_deadline_ = this->now_() + RANGE_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_RANGE_START;
  } else if (this->lux_pending_) {
    this->lux_pending_ = false;
    this->state_deadline_ = this->now_() + this->als_integration_time_ms_ + ALS_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_ALS_START;
  } else {
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
  }
}

// ---------------------------------------------------------------------------
// State machine — ALS start
// ---------------------------------------------------------------------------

void VL6180XComponent::do_als_start_() {
  if (this->recovery_pause_) {
    this->range_pending_ = false;
    this->lux_pending_ = false;
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
    return;
  }

  if (this->range_pending_) {
    this->lux_pending_ = true;
    this->range_pending_ = false;
    this->state_deadline_ = this->now_() + RANGE_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_RANGE_START;
    return;
  }

  uint8_t icfg = 0;
  if (!this->read_reg8_(REG_SYSTEM_INTERRUPT_CONFIG, icfg)) {
    this->fail_lux_measurement_();
    return;
  }
  icfg = (icfg & ~0x38) | (0x04 << 3);
  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CONFIG, icfg)) {
    this->fail_lux_measurement_();
    return;
  }

  if (!this->write_reg8_(REG_SYSALS_ANALOGUE_GAIN, 0x40 | this->als_gain_)) {
    this->fail_lux_measurement_();
    return;
  }

  uint8_t status = 0;
  if (!this->read_reg8_(REG_RESULT_ALS_STATUS, status)) {
    this->fail_lux_measurement_();
    return;
  }
  if (!(status & 0x01)) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      ESP_LOGW(TAG, "[%s] ALS: device not ready (0x%02X)", this->name_str_, status);
      this->fail_lux_measurement_();
    }
    return;
  }

  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x07)) {
    this->fail_lux_measurement_();
    return;
  }
  if (!this->write_reg8_(REG_SYSALS_START, 0x01)) {
    this->fail_lux_measurement_();
    return;
  }

  this->state_deadline_ = this->now_() + this->als_integration_time_ms_ + ALS_INTEGRATION_MARGIN_MS;
  this->state_ = MeasurementState::MEASUREMENT_STATE_ALS_WAIT;
}

// ---------------------------------------------------------------------------
// State machine — ALS wait
// ---------------------------------------------------------------------------

void VL6180XComponent::do_als_wait_() {
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      uint8_t irq = 0;
      if (!this->read_reg8_(REG_RESULT_INTERRUPT_STATUS_GPIO, irq)) {
        this->fail_lux_measurement_();
        return;
      }
      ESP_LOGW(TAG, "[%s] ALS: interrupt timeout (irq=0x%02X)", this->name_str_, irq);
      this->fail_lux_measurement_();
    }
    return;
  }

  uint8_t irq = 0;
  if (!this->read_reg8_(REG_RESULT_INTERRUPT_STATUS_GPIO, irq)) {
    this->fail_lux_measurement_();
    return;
  }

  if (((irq >> 3) & 0x07) != 4) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      ESP_LOGW(TAG, "[%s] ALS: timeout (irq=0x%02X)", this->name_str_, irq);
      this->fail_lux_measurement_();
    }
    return;
  }

  uint16_t raw = 0;
  uint8_t status = 0;
  if (!this->read_reg8_(REG_RESULT_ALS_STATUS, status)) {
    this->fail_lux_measurement_();
    return;
  }
  const uint8_t als_error = (status & ALS_ERROR_MASK) >> 4;
  if (als_error != 0) {
    ESP_LOGW(TAG, "[%s] ALS result error: %u (status=0x%02X)", this->name_str_, als_error, status);
    this->fail_lux_measurement_();
    return;
  }
  if (!this->read_reg16_(REG_RESULT_ALS_VAL, raw)) {
    this->fail_lux_measurement_();
    return;
  }
  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x07)) {
    this->fail_lux_measurement_();
    return;
  }

  this->publish_lux_(raw);
  if (this->state_ == MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    return;
  if (this->recovery_pause_) {
    this->range_pending_ = false;
    this->lux_pending_ = false;
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
    return;
  }
  if (this->range_pending_) {
    this->range_pending_ = false;
    this->state_deadline_ = this->now_() + RANGE_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_RANGE_START;
    return;
  }
  if (this->lux_pending_) {
    this->lux_pending_ = false;
    this->state_deadline_ = this->now_() + this->als_integration_time_ms_ + ALS_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_ALS_START;
    return;
  }
  // publish_lux_ calls record_success_() — if that somehow triggered recovery
  // (it won't, but be defensive) don't overwrite the state
  if (this->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
}

// ---------------------------------------------------------------------------
// State machine — recovery wait
//
// All sensors that hit the failure threshold enter this state. Only the
// failed sensor that wins the bus-scoped recovery lock drives reinitialization.
// It pauses queued work on sibling sensors until active measurements finish.
// ---------------------------------------------------------------------------

bool VL6180XComponent::begin_recovery_attempt_() {
  if (this->recovery_attempt_active_)
    return true;

  auto &all = all_instances();
  for (auto *inst : all) {
    if (inst != this && inst->bus_ == this->bus_ && inst->recovery_attempt_active_)
      return false;
  }

  this->recovery_attempt_active_ = true;
  for (auto *inst : all) {
    if (inst == this || inst->bus_ != this->bus_)
      continue;
    inst->recovery_pause_ = true;
    inst->range_pending_ = false;
    inst->lux_pending_ = false;
    if (inst->state_ == MeasurementState::MEASUREMENT_STATE_RANGE_START ||
        inst->state_ == MeasurementState::MEASUREMENT_STATE_ALS_START)
      inst->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
  }
  return true;
}

void VL6180XComponent::end_recovery_attempt_() {
  for (auto *inst : all_instances()) {
    if (inst->bus_ == this->bus_)
      inst->recovery_pause_ = false;
  }
  this->recovery_attempt_active_ = false;
}

bool VL6180XComponent::recovery_group_has_active_measurement_() const {
  for (auto *inst : all_instances()) {
    if (inst == this || inst->bus_ != this->bus_)
      continue;
    if (inst->state_ == MeasurementState::MEASUREMENT_STATE_RANGE_WAIT ||
        inst->state_ == MeasurementState::MEASUREMENT_STATE_ALS_WAIT)
      return true;
  }
  return false;
}

bool VL6180XComponent::recover_sensor_() {
  auto &all = all_instances();
  VL6180XComponent *default_address_sensor = nullptr;
  const uint8_t target_address = this->get_i2c_address();

  if (target_address != 0x29) {
    for (auto *inst : all) {
      if (inst != this && inst->bus_ == this->bus_ && inst->get_i2c_address() == 0x29) {
        default_address_sensor = inst;
        break;
      }
    }
  }

  if (default_address_sensor != nullptr && is_measuring(default_address_sensor->state_))
    return false;
  if (default_address_sensor != nullptr && default_address_sensor->enable_pin_ == nullptr)
    return false;
  if (default_address_sensor != nullptr)
    default_address_sensor->enable_pin_->digital_write(false);
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(false);
    delay(10);
    this->enable_pin_->digital_write(true);
    delay(10);
  }

  this->set_i2c_address(0x29);
  bool initialized_at_default = this->init_sensor_();
  bool initialized = initialized_at_default;
  if (!initialized && target_address != 0x29) {
    this->set_i2c_address(target_address);
    initialized = this->init_sensor_();
  }
  if (initialized && initialized_at_default && target_address != 0x29)
    initialized = this->assign_address_(target_address);

  if (!initialized && target_address != 0x29 && this->enable_pin_ != nullptr)
    this->enable_pin_->digital_write(false);
  this->set_i2c_address(target_address);
  this->initialized_ = initialized;
  if (initialized) {
    this->consecutive_failures_ = 0;
    this->reinit_attempts_ = 0;
    this->retry_limit_reached_ = false;
    this->status_clear_warning();
    this->lux_pending_ = false;
    this->range_pending_ = false;
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
  }

  if (default_address_sensor != nullptr) {
    default_address_sensor->enable_pin_->digital_write(true);
    delay(10);
    default_address_sensor->initialized_ = default_address_sensor->init_sensor_();
    if (default_address_sensor->initialized_)
      default_address_sensor->status_clear_warning();
    default_address_sensor->state_ = default_address_sensor->initialized_
                                         ? MeasurementState::MEASUREMENT_STATE_IDLE
                                         : MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
    if (!default_address_sensor->initialized_) {
      default_address_sensor->publish_range_error_(RangeStatus::RANGE_STATUS_RECOVERING);
      default_address_sensor->reinit_due_at_ =
          default_address_sensor->now_() + default_address_sensor->retry_interval_ms_;
      default_address_sensor->reinit_attempts_ = 0;
    }
  }
  return initialized;
}

void VL6180XComponent::do_recovery_wait_() {
  if (this->recovery_pause_)
    return;
  if (!deadline_reached(this->now_(), this->reinit_due_at_))
    return;

  if (!this->begin_recovery_attempt_()) {
    this->reinit_due_at_ = this->now_() + RECOVERY_COORDINATION_DELAY_MS;
    return;
  }
  if (this->recovery_group_has_active_measurement_()) {
    this->end_recovery_attempt_();
    this->reinit_due_at_ = this->now_() + RECOVERY_COORDINATION_DELAY_MS;
    return;
  }

  const bool recovered = this->recover_sensor_();
  this->end_recovery_attempt_();
  if (recovered) {
    ESP_LOGI(TAG, "[%s] Recovery successful", this->name_str_);
    return;
  }
  if (this->reinit_attempts_ < UINT8_MAX)
    this->reinit_attempts_++;

  if (!this->retry_limit_reached_ && this->reinit_attempts_ >= this->max_retries_) {
    this->retry_limit_reached_ = true;
    PUBLISH_STATE(this->range_sensor_, NAN);
    PUBLISH_STATE(this->range_status_text_sensor_,
                  RangeStatusStrings::get_progmem_str(static_cast<uint8_t>(RangeStatus::RANGE_STATUS_FAILED),
                                                      RangeStatusStrings::LAST_INDEX));
    PUBLISH_STATE(this->lux_sensor_, NAN);
    ESP_LOGE(TAG, "[%s] Recovery failed %u times; continuing periodic retries", this->name_str_,
             static_cast<unsigned>(this->reinit_attempts_));
  }

  this->reinit_due_at_ = this->now_() + this->retry_interval_ms_;
  this->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
  ESP_LOGW(TAG, "[%s] Reinitialization attempt %u failed; retrying in %lu s", this->name_str_,
           static_cast<unsigned>(this->reinit_attempts_), static_cast<unsigned long>(this->retry_interval_ms_ / 1000U));
}

// ---------------------------------------------------------------------------
// lux_start_ — called by the independent lux interval timer
// ---------------------------------------------------------------------------

void VL6180XComponent::lux_start_() {
  if (this->is_failed() || this->retry_limit_reached_ || this->recovery_pause_)
    return;
  if (this->lux_sensor_ == nullptr)
    return;
  if (this->state_ == MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    return;

  if (this->state_ == MeasurementState::MEASUREMENT_STATE_IDLE) {
    this->state_deadline_ = this->now_() + this->als_integration_time_ms_ + ALS_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_ALS_START;
  } else {
    this->lux_pending_ = true;
  }
}

// ---------------------------------------------------------------------------
// ESPHome lifecycle — setup()
// ---------------------------------------------------------------------------

void VL6180XComponent::setup() {
  auto &all = all_instances();

  if (this->lux_sensor_ != nullptr) {
    const uint32_t lux_ms =
        (this->lux_update_interval_ms_ > 0) ? this->lux_update_interval_ms_ : this->get_update_interval();
    this->set_interval(LUX_INTERVAL_ID, lux_ms, [this]() { this->lux_start_(); });
    ESP_LOGD(TAG, "[%s] Lux interval: %lu ms", this->name_str_, static_cast<unsigned long>(lux_ms));
  }

  if (std::find_if(all.begin(), all.end(), [this](const auto *inst) {
        return inst != this && inst->bus_ == this->bus_ && inst->startup_sequence_complete_;
      }) != all.end())
    return;

  const auto sensor_count = static_cast<size_t>(
      std::count_if(all.begin(), all.end(), [this](const auto *inst) { return inst->bus_ == this->bus_; }));
  ESP_LOGI(TAG, "VL6180X: initialising %u sensor(s) on this I2C bus", static_cast<unsigned>(sensor_count));

  // GPIO setup — must happen before run_init_sequence drives the pins
  for (auto *inst : all) {
    if (inst->bus_ != this->bus_)
      continue;
    if (inst->enable_pin_ != nullptr)
      inst->enable_pin_->setup();
    if (inst->interrupt_pin_ != nullptr)
      inst->interrupt_pin_->setup();
  }

  run_init_sequence(this->bus_);

  // Sensors that failed initial boot enter RECOVERY_WAIT immediately so
  // they'll be retried automatically without manual intervention.
  for (auto *inst : all) {
    if (inst->bus_ != this->bus_)
      continue;
    if (!inst->initialized_ && !inst->is_failed()) {
      inst->publish_range_error_(RangeStatus::RANGE_STATUS_RECOVERING);
      inst->reinit_attempts_ = 0;
      inst->reinit_due_at_ = inst->now_() + inst->retry_interval_ms_;
      inst->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
      ESP_LOGW(TAG, "[%s] Initial boot failed — will retry in %lu s", inst->name_str_,
               static_cast<unsigned long>(inst->retry_interval_ms_ / 1000U));
    }
    inst->startup_sequence_complete_ = true;
  }

  ESP_LOGI(TAG, "VL6180X: init sequence complete");
}

// ---------------------------------------------------------------------------
// loop() — advances the state machine on every main-loop iteration
// ---------------------------------------------------------------------------

void VL6180XComponent::loop() {
  if (this->is_failed())
    return;
  switch (this->state_) {
    case MeasurementState::MEASUREMENT_STATE_IDLE:
      break;
    case MeasurementState::MEASUREMENT_STATE_RANGE_START:
      this->do_range_start_();
      break;
    case MeasurementState::MEASUREMENT_STATE_RANGE_WAIT:
      this->do_range_wait_();
      break;
    case MeasurementState::MEASUREMENT_STATE_ALS_START:
      this->do_als_start_();
      break;
    case MeasurementState::MEASUREMENT_STATE_ALS_WAIT:
      this->do_als_wait_();
      break;
    case MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT:
      this->do_recovery_wait_();
      break;
  }
}

// ---------------------------------------------------------------------------
// update() — kicks off a range measurement cycle on the configured interval
// ---------------------------------------------------------------------------

void VL6180XComponent::update() {
  if (this->is_failed() || this->retry_limit_reached_ || this->recovery_pause_)
    return;
  if (this->range_sensor_ == nullptr && this->range_status_text_sensor_ == nullptr)
    return;
  if (this->state_ == MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    return;

  if (this->state_ != MeasurementState::MEASUREMENT_STATE_IDLE) {
    if (this->state_ == MeasurementState::MEASUREMENT_STATE_ALS_START ||
        this->state_ == MeasurementState::MEASUREMENT_STATE_ALS_WAIT) {
      this->range_pending_ = true;
    }
    return;
  }

  this->state_deadline_ = this->now_() + RANGE_READY_TIMEOUT_MS;
  this->state_ = MeasurementState::MEASUREMENT_STATE_RANGE_START;
}

// ---------------------------------------------------------------------------
// dump_config
// ---------------------------------------------------------------------------

void VL6180XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "VL6180X '%s':", this->name_str_);
  LOG_I2C_DEVICE(this);
  if (this->enable_pin_ != nullptr) {
    LOG_PIN("  Enable Pin: ", this->enable_pin_);
  }
  if (this->interrupt_pin_ != nullptr) {
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  }
  LOG_SENSOR("  ", "Range", this->range_sensor_);
  LOG_TEXT_SENSOR("  ", "Range Status", this->range_status_text_sensor_);
  LOG_SENSOR("  ", "Lux", this->lux_sensor_);
  ESP_LOGCONFIG(TAG, "  Recovery: threshold=%u, retry_interval=%lus, max_retries=%u",
                static_cast<unsigned>(this->failure_threshold_),
                static_cast<unsigned long>(this->retry_interval_ms_ / 1000U),
                static_cast<unsigned>(this->max_retries_));
  LOG_UPDATE_INTERVAL(this);
  if (this->retry_limit_reached_) {
    ESP_LOGE(TAG, "  Recovery retry limit reached; periodic retries continue");
  } else if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with VL6180X failed during setup");
  }
}

}  // namespace esphome::vl6180x
