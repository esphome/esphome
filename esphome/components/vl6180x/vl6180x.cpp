#include "vl6180x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <algorithm>

namespace esphome::vl6180x {

ESPHOME_LOG_TAG(TAG, "vl6180x");
static constexpr uint32_t LUX_INTERVAL_ID = 0;

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
static constexpr uint16_t REG_RESULT_ALS_VAL = 0x050;
static constexpr uint16_t REG_RESULT_RANGE_VAL = 0x062;
static constexpr uint16_t REG_RESULT_ALS_STATUS = 0x04E;
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
static constexpr uint32_t ALS_RESULT_TIMEOUT_MS = 500;  // 100ms integration + margin

// ---------------------------------------------------------------------------
// Low-level I2C — 16-bit register addressing, MSB first
// ---------------------------------------------------------------------------

bool VL6180XComponent::write_reg8_(uint16_t reg, uint8_t value) {
  uint8_t buf[3] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF), value};
  i2c::ErrorCode err = this->write(buf, 3);
  const bool ok = this->status_comm_result(err == i2c::ERROR_OK);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] write_reg8 0x%04X=0x%02X err=%d", this->name_str_, reg, value, (int) err);
  }
  return ok;
}

bool VL6180XComponent::write_reg16_(uint16_t reg, uint16_t value) {
  uint8_t buf[4] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF), (uint8_t) (value >> 8), (uint8_t) (value & 0xFF)};
  i2c::ErrorCode err = this->write(buf, 4);
  const bool ok = this->status_comm_result(err == i2c::ERROR_OK);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] write_reg16 0x%04X=0x%04X err=%d", this->name_str_, reg, value, (int) err);
  }
  return ok;
}

bool VL6180XComponent::read_reg8_(uint16_t reg, uint8_t &value) {
  uint8_t addr[2] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF)};
  i2c::ErrorCode err = this->write_read(addr, 2, &value, 1);
  const bool ok = this->status_comm_result(err == i2c::ERROR_OK);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] read_reg8 0x%04X err=%d", this->name_str_, reg, (int) err);
  }
  return ok;
}

bool VL6180XComponent::read_reg16_(uint16_t reg, uint16_t &value) {
  uint8_t addr[2] = {(uint8_t) (reg >> 8), (uint8_t) (reg & 0xFF)};
  uint8_t data[2];
  i2c::ErrorCode err = this->write_read(addr, 2, data, 2);
  const bool ok = this->status_comm_result(err == i2c::ERROR_OK);
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
  if (!this->write_reg8_(0x001B, 0x09))
    return false;  // VHV repeat rate
  if (!this->write_reg8_(REG_SYSALS_INTEGRATION_PERIOD_HI, 0x00))
    return false;
  if (!this->write_reg8_(REG_SYSALS_INTEGRATION_PERIOD_LO, 100))
    return false;  // 100ms integration
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

void VL6180XComponent::run_init_sequence() {
  auto &all = all_instances();

  // Reset all state machines — any in-progress measurement is abandoned.
  // Publish NAN / "Recovering" so HA reflects the outage immediately.
  for (auto *inst : all) {
    if (inst->is_failed())
      continue;
    if (inst->state_ != MeasurementState::MEASUREMENT_STATE_IDLE &&
        inst->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT) {
      inst->publish_range_error_("Recovering");
    }
    inst->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
    inst->lux_pending_ = false;
    inst->range_pending_ = false;
  }

  // Step 1: all XSHUT pins low
  for (auto *inst : all) {
    if (inst->enable_pin_ != nullptr)
      inst->enable_pin_->digital_write(false);
  }
  delay(10);

  // Step 2: initialize non-default addresses first, then 0x29 sensors. Keeping
  // the registry in place avoids stable_sort's temporary allocation during
  // recovery, which can run after setup on a long-lived device.
  for (uint8_t phase = 0; phase < 2; phase++) {
    for (auto *inst : all) {
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
  if (this->consecutive_failures_ >= this->failure_threshold_ &&
      this->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT) {
    ESP_LOGW(TAG, "[%s] %d consecutive failures — entering recovery", this->name_str_, this->consecutive_failures_);
    this->reinit_attempts_ = 0;
    this->reinit_due_at_ = this->now_() + this->retry_interval_ms_;
    this->initialized_ = false;
    this->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
    auto &all = all_instances();
    if (!all.empty() && all[0] != this && all[0]->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT) {
      all[0]->reinit_attempts_ = 0;
      all[0]->reinit_due_at_ = this->now_() + all[0]->retry_interval_ms_;
      all[0]->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
    }
    // Do not call publish_range_error_() here: it calls record_failure_(),
    // which would recurse until the ESP32 loop task overflows its stack.
    if (this->range_sensor_ != nullptr)
      this->range_sensor_->publish_state(NAN);
    if (this->range_status_sensor_ != nullptr)
      this->range_status_sensor_->publish_state("Recovering");
    if (this->lux_sensor_ != nullptr)
      this->lux_sensor_->publish_state(NAN);
  }
}

void VL6180XComponent::record_success_() { this->consecutive_failures_ = 0; }

void VL6180XComponent::fail_range_measurement_(const char *status) {
  this->publish_range_error_(status);
  if (this->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
}

void VL6180XComponent::fail_lux_measurement_() {
  if (this->lux_sensor_ != nullptr)
    this->lux_sensor_->publish_state(NAN);
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
  const char *status = "OK";

  if (error_code != 0) {
    const char *log_str;
    bool is_sensor_failure = false;
    switch (error_code) {
      case 1:
        status = "Hardware Fault";
        log_str = "VCSEL continuity";
        is_sensor_failure = true;
        break;
      case 2:
        status = "Hardware Fault";
        log_str = "VCSEL watchdog test";
        is_sensor_failure = true;
        break;
      case 3:
        status = "Hardware Fault";
        log_str = "VCSEL watchdog";
        is_sensor_failure = true;
        break;
      case 4:
        status = "Hardware Fault";
        log_str = "PLL1 lock";
        is_sensor_failure = true;
        break;
      case 5:
        status = "Hardware Fault";
        log_str = "PLL2 lock";
        is_sensor_failure = true;
        break;
      case 6:
        status = "Target Too Close";
        log_str = "early convergence";
        is_sensor_failure = false;
        break;
      case 7:
        status = "No Target";
        log_str = "max convergence";
        is_sensor_failure = false;
        break;
      case 8:
        status = "No Target";
        log_str = "no target (ignored)";
        is_sensor_failure = false;
        break;
      case 11:
        status = "No Target";
        log_str = "no target (SNR)";
        is_sensor_failure = false;
        break;
      case 12:
        status = "Target Too Close";
        log_str = "raw underflow";
        is_sensor_failure = false;
        break;
      case 13:
        status = "Out of Range";
        log_str = "raw overflow";
        is_sensor_failure = false;
        break;
      case 14:
        status = "Target Too Close";
        log_str = "algo underflow";
        is_sensor_failure = false;
        break;
      case 15:
        status = "Out of Range";
        log_str = "algo overflow";
        is_sensor_failure = false;
        break;
      default:
        status = "Sensor Error";
        log_str = "unknown";
        is_sensor_failure = true;
        break;
    }
    ESP_LOGW(TAG, "[%s] Range error %d: %s (raw=%d)", this->name_str_, error_code, log_str, raw);
    if (this->range_sensor_ != nullptr)
      this->range_sensor_->publish_state(NAN);
    if (this->range_status_sensor_ != nullptr)
      this->range_status_sensor_->publish_state(status);

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
  if (this->range_sensor_ != nullptr)
    this->range_sensor_->publish_state(distance_mm);
  if (this->range_status_sensor_ != nullptr)
    this->range_status_sensor_->publish_state(status);
  this->record_success_();
}

void VL6180XComponent::publish_range_error_(const char *status) {
  // Called for I2C-level errors and timeouts — always a sensor failure
  if (this->range_sensor_ != nullptr)
    this->range_sensor_->publish_state(NAN);
  if (this->range_status_sensor_ != nullptr)
    this->range_status_sensor_->publish_state(status);
  this->record_failure_();
}

void VL6180XComponent::publish_lux_(uint16_t raw) {
  const float lux = 0.32f * static_cast<float>(raw) * 100.0f / (this->als_gain_factor_ * 101.0f);
  ESP_LOGD(TAG, "[%s] Lux raw=%u → %.2f lx", this->name_str_, static_cast<unsigned>(raw), lux);
  if (this->lux_sensor_ != nullptr)
    this->lux_sensor_->publish_state(lux);
  this->record_success_();
}

// ---------------------------------------------------------------------------
// State machine — range start
// ---------------------------------------------------------------------------

void VL6180XComponent::do_range_start_() {
  uint8_t icfg = 0;
  if (!this->read_reg8_(REG_SYSTEM_INTERRUPT_CONFIG, icfg)) {
    this->fail_range_measurement_("Sensor Error");
    return;
  }
  icfg = (icfg & ~0x07) | 0x04;
  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CONFIG, icfg)) {
    this->fail_range_measurement_("Sensor Error");
    return;
  }

  uint8_t status = 0;
  if (!this->read_reg8_(REG_RESULT_ALS_STATUS, status)) {
    this->fail_range_measurement_("Sensor Error");
    return;
  }
  if (!(status & 0x01)) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      ESP_LOGW(TAG, "[%s] Range: device not ready (0x%02X)", this->name_str_, status);
      this->fail_range_measurement_("Timeout");
    }
    return;
  }

  if (!this->write_reg8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x07)) {
    this->fail_range_measurement_("Sensor Error");
    return;
  }
  if (!this->write_reg8_(REG_SYSRANGE_START, 0x01)) {
    this->fail_range_measurement_("Sensor Error");
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
        this->fail_range_measurement_("Sensor Error");
        return;
      }
      ESP_LOGW(TAG, "[%s] Range: interrupt timeout (irq=0x%02X)", this->name_str_, irq);
      this->fail_range_measurement_("Timeout");
    }
    return;
  }

  uint8_t irq = 0;
  if (!this->read_reg8_(REG_RESULT_INTERRUPT_STATUS_GPIO, irq)) {
    this->fail_range_measurement_("Sensor Error");
    return;
  }

  if (!(irq & 0x04)) {
    if (deadline_reached(this->now_(), this->state_deadline_)) {
      ESP_LOGW(TAG, "[%s] Range: timeout (irq=0x%02X)", this->name_str_, irq);
      this->fail_range_measurement_("Timeout");
    }
    return;
  }

  uint8_t raw = 0, range_status = 0;
  bool ok = this->read_reg8_(REG_RESULT_RANGE_VAL, raw) && this->read_reg8_(REG_RESULT_RANGE_STATUS, range_status);
  ok = this->write_reg8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x07) && ok;

  if (!ok) {
    this->fail_range_measurement_("Sensor Error");
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

  if (this->lux_pending_) {
    this->lux_pending_ = false;
    this->state_deadline_ = this->now_() + ALS_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_ALS_START;
  } else if (this->range_pending_) {
    this->range_pending_ = false;
    this->state_deadline_ = this->now_() + RANGE_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_RANGE_START;
  } else {
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
  }
}

// ---------------------------------------------------------------------------
// State machine — ALS start
// ---------------------------------------------------------------------------

void VL6180XComponent::do_als_start_() {
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
  if (!this->read_reg8_(REG_RESULT_RANGE_STATUS, status)) {
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

  this->state_deadline_ = this->now_() + ALS_RESULT_TIMEOUT_MS;
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
  if (this->lux_pending_) {
    this->lux_pending_ = false;
    this->state_deadline_ = this->now_() + ALS_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_ALS_START;
    return;
  }
  if (this->range_pending_) {
    this->range_pending_ = false;
    this->state_deadline_ = this->now_() + RANGE_READY_TIMEOUT_MS;
    this->state_ = MeasurementState::MEASUREMENT_STATE_RANGE_START;
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
// instance at all_instances()[0] (the designated initiator) drives the
// actual reinit. Non-initiators simply wait here until the initiator has
// completed reinit (detected via initialized_ being restored to true),
// then return to IDLE.
// ---------------------------------------------------------------------------

void VL6180XComponent::do_recovery_wait_() {
  auto &all = all_instances();

  // ── Non-initiator path ───────────────────────────────────────────────────
  // If the initiator has already completed reinit and restored our
  // initialized_ flag, we can return to IDLE — reinit covered us.
  if (!all.empty() && all[0] != this) {
    if (this->initialized_) {
      this->consecutive_failures_ = 0;
      this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
      ESP_LOGI(TAG, "[%s] Recovery complete (handled by initiator)", this->name_str_);
    } else if (all[0]->state_ != MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT) {
      all[0]->reinit_attempts_ = 0;
      all[0]->reinit_due_at_ = this->now_() + all[0]->retry_interval_ms_;
      all[0]->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
    }
    // else: still waiting for initiator — come back next loop()
    return;
  }

  // ── Initiator path ───────────────────────────────────────────────────────

  // Not time yet for this attempt
  if (!deadline_reached(this->now_(), this->reinit_due_at_))
    return;

  // Wait for any sensor still mid-measurement to finish naturally.
  // Only block on active measurement states — sensors in IDLE or RECOVERY_WAIT
  // are fine to proceed past.
  for (auto *inst : all) {
    if (is_measuring(inst->state_))
      return;  // come back next loop()
  }

  // All sensors are idle or in recovery — safe to reinit
  const uint16_t attempt_number = static_cast<uint16_t>(this->reinit_attempts_) + 1;
  ESP_LOGI(TAG, "VL6180X: reinit attempt %u/%d", static_cast<unsigned>(attempt_number), (int) this->max_retries_);
  run_init_sequence();

  const bool all_initialized =
      std::all_of(all.begin(), all.end(), [](const VL6180XComponent *inst) { return inst->initialized_; });
  if (all_initialized) {
    this->consecutive_failures_ = 0;
    this->reinit_attempts_ = 0;
    this->state_ = MeasurementState::MEASUREMENT_STATE_IDLE;
    ESP_LOGI(TAG, "[%s] Recovery successful after %u attempt(s)", this->name_str_,
             static_cast<unsigned>(attempt_number));
    return;
  }

  // Reinit failed for this sensor
  if (this->reinit_attempts_ < this->max_retries_)
    this->reinit_attempts_++;
  const bool retry_limit_reached = this->reinit_attempts_ >= this->max_retries_;
  // Never permanently mark a recoverable sensor failed. A VL6180X that
  // remains busy after a transient bus/power disturbance must keep polling
  // recovery rather than disappear until the ESP itself reboots.
  // Keep ordinary retries at the configured interval, then back off to 30 s
  // after the configured retry budget to avoid continuously hammering I2C.
  const uint32_t retry_delay_ms =
      retry_limit_reached ? std::max<uint32_t>(this->retry_interval_ms_, 30000U) : this->retry_interval_ms_;
  ESP_LOGW(TAG, "[%s] Reinit attempt %d failed; retrying in %lu s%s", this->name_str_, (int) this->reinit_attempts_,
           static_cast<unsigned long>(retry_delay_ms / 1000U),
           retry_limit_reached ? LOG_STR_LITERAL(" (backoff; recovery remains active)") : LOG_STR_LITERAL(""));
  this->reinit_due_at_ = this->now_() + retry_delay_ms;
  this->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
  // Stay in RECOVERY_WAIT and keep retrying until the sensor responds.
}

// ---------------------------------------------------------------------------
// lux_start_ — called by the independent lux interval timer
// ---------------------------------------------------------------------------

void VL6180XComponent::lux_start_() {
  if (this->is_failed())
    return;
  if (this->lux_sensor_ == nullptr)
    return;
  if (this->state_ == MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT)
    return;

  if (this->state_ == MeasurementState::MEASUREMENT_STATE_IDLE) {
    this->state_deadline_ = this->now_() + ALS_READY_TIMEOUT_MS;
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

  static bool sequence_done = false;
  if (sequence_done)
    return;
  if (!all.empty() && all[0] != this)
    return;
  sequence_done = true;

  ESP_LOGI(TAG, "VL6180X: initialising %d sensor(s)", (int) all.size());

  // GPIO setup — must happen before run_init_sequence drives the pins
  for (auto *inst : all) {
    if (inst->enable_pin_ != nullptr)
      inst->enable_pin_->setup();
    if (inst->interrupt_pin_ != nullptr)
      inst->interrupt_pin_->setup();
  }

  run_init_sequence();

  // Sensors that failed initial boot enter RECOVERY_WAIT immediately so
  // they'll be retried automatically without manual intervention.
  for (auto *inst : all) {
    if (!inst->initialized_ && !inst->is_failed()) {
      inst->publish_range_error_("Recovering");
      inst->reinit_attempts_ = 0;
      inst->reinit_due_at_ = inst->now_() + inst->retry_interval_ms_;
      inst->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
      ESP_LOGW(TAG, "[%s] Initial boot failed — will retry in %lu s", inst->name_str_,
               static_cast<unsigned long>(inst->retry_interval_ms_ / 1000U));
    }
  }

  if (!all.empty() && !all[0]->initialized_) {
    all[0]->reinit_attempts_ = 0;
    all[0]->reinit_due_at_ = all[0]->now_() + all[0]->retry_interval_ms_;
    all[0]->state_ = MeasurementState::MEASUREMENT_STATE_RECOVERY_WAIT;
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
  if (this->is_failed())
    return;
  if (this->range_sensor_ == nullptr && this->range_status_sensor_ == nullptr)
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
  LOG_TEXT_SENSOR("  ", "Range Status", this->range_status_sensor_);
  LOG_SENSOR("  ", "Lux", this->lux_sensor_);
  ESP_LOGCONFIG(TAG, "  Recovery: threshold=%d, retry_interval=%lus, max_retries=%d", (int) this->failure_threshold_,
                static_cast<unsigned long>(this->retry_interval_ms_ / 1000U), (int) this->max_retries_);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with VL6180X failed permanently!");
  }
}

}  // namespace esphome::vl6180x
