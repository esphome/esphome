#include "m5_unit_bldc.h"

#include <cmath>
#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::m5_unit_bldc {

ESPHOME_LOG_TAG(TAG, "m5_unit_bldc");

// Index matches the raw REG_MOTOR_STATUS byte; any value past MOTOR_STATUS_UNKNOWN falls back to it.
PROGMEM_STRING_TABLE(MotorStatusStrings, "Standby", "Running", "Error", "Unknown");
static constexpr uint8_t MOTOR_STATUS_UNKNOWN = 3;

bool M5UnitBldc::write_float_(uint8_t reg, float value) {
  uint8_t buf[4];
  std::memcpy(buf, &value, 4);
  return this->write_register(reg, buf, 4) == i2c::ERROR_OK;
}

bool M5UnitBldc::read_float_(uint8_t reg, float *value) {
  uint8_t buf[4];
  if (this->read_register(reg, buf, 4) != i2c::ERROR_OK)
    return false;
  std::memcpy(value, buf, 4);
  return true;
}

void M5UnitBldc::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Unit-BLDC...");

  uint8_t mode = static_cast<uint8_t>(this->control_mode_);
  if (this->write_register(REG_MODE, &mode, 1) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to write control mode"));
    return;
  }

  uint8_t direction = static_cast<uint8_t>(this->initial_direction_);
  if (this->write_register(REG_DIRECTION, &direction, 1) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to write initial direction"));
    return;
  }

  if (this->direction_select_ != nullptr) {
    this->direction_select_->publish_state(this->initial_direction_ == Direction::DIRECTION_BACKWARD ? "Backward"
                                                                                                     : "Forward");
  }

  // Read the previously-active values back before overwriting them, so the save-to-flash check
  // below compares against what the device actually had, not against what this boot just wrote.
  uint8_t motor_config[2] = {static_cast<uint8_t>(this->motor_model_), this->pole_pairs_};
  uint8_t previous_motor_config[2];
  bool had_previous_motor_config = this->read_register(REG_MOTOR_CONFIG, previous_motor_config, 2) == i2c::ERROR_OK;
  if (this->write_register(REG_MOTOR_CONFIG, motor_config, 2) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to write motor model/pole pairs"));
    return;
  }

  // The device's own MCU keeps running -- and keeps driving whatever PWM/RPM it was last given --
  // across an ESP reboot. Read that back so `last_pwm_`/`last_target_rpm_` reflect reality (instead
  // of restarting a reversal from a stale zero) and so the `number` entities come up showing the
  // motor's actual command instead of unknown. A failed read is not fatal: it just leaves the
  // entity unpublished, same as any other runtime read failure.
  uint16_t pwm_raw;
  if (this->read_pwm_raw_(&pwm_raw)) {
    this->last_pwm_ = pwm_raw;
    if (this->pwm_number_ != nullptr)
      this->pwm_number_->publish_state(pwm_raw);
  }

  float target_rpm;
  if (this->read_float_(REG_SET_RPM, &target_rpm)) {
    this->last_target_rpm_ = target_rpm;
    if (this->target_rpm_number_ != nullptr)
      this->target_rpm_number_->publish_state(target_rpm);
  }

  uint8_t pid[12];
  uint8_t previous_pid[12];
  bool had_previous_pid = false;
  if (this->has_pid_) {
    had_previous_pid = this->read_register(REG_PID, previous_pid, 12) == i2c::ERROR_OK;

    int32_t p_int = lroundf(this->p_ * 100);
    int32_t i_int = lroundf(this->i_ * 100);
    int32_t d_int = lroundf(this->d_ * 100);
    std::memcpy(pid, &p_int, 4);
    std::memcpy(pid + 4, &i_int, 4);
    std::memcpy(pid + 8, &d_int, 4);
    if (this->write_register(REG_PID, pid, 12) != i2c::ERROR_OK) {
      this->mark_failed(LOG_STR("Failed to write PID gains"));
      return;
    }
  }

  if (this->save_to_flash_) {
    // The device only has a limited number of flash erase/write cycles, so only persist when the
    // values just written above actually differ from what the device had before this boot;
    // otherwise every ESP reboot, OTA or crash loop would needlessly wear out the unit's flash.
    bool needs_save = !had_previous_motor_config || std::memcmp(previous_motor_config, motor_config, 2) != 0;
    if (!needs_save && this->has_pid_) {
      needs_save = !had_previous_pid || std::memcmp(previous_pid, pid, 12) != 0;
    }

    if (needs_save) {
      uint8_t save = 1;
      this->write_register(REG_SAVE_TO_FLASH, &save, 1);
    }
  }
}

void M5UnitBldc::update() {
  bool read_failed = false;

  if (this->rpm_sensor_ != nullptr) {
    float rpm;
    if (this->read_float_(REG_READBACK_RPM, &rpm)) {
      this->rpm_sensor_->publish_state(rpm);
    } else {
      read_failed = true;
    }
  }

  if (this->frequency_sensor_ != nullptr) {
    float freq;
    if (this->read_float_(REG_READBACK_FREQ, &freq)) {
      this->frequency_sensor_->publish_state(freq);
    } else {
      read_failed = true;
    }
  }

  if (this->status_text_sensor_ != nullptr) {
    uint8_t status;
    if (this->read_register(REG_MOTOR_STATUS, &status, 1) == i2c::ERROR_OK) {
      ProgmemStr name = MotorStatusStrings::get_progmem_str(status, MOTOR_STATUS_UNKNOWN);
      char text[16];
      ESPHOME_strncpy_P(text, reinterpret_cast<ESPHOME_PGM_P>(name), sizeof(text));
      text[sizeof(text) - 1] = '\0';
      this->status_text_sensor_->publish_state(text);
    } else {
      read_failed = true;
    }
  }

  if (read_failed) {
    this->status_set_warning(LOG_STR("Failed to read motor status"));
  } else {
    this->status_clear_warning();
  }
}

void M5UnitBldc::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Unit-BLDC:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Control mode: %s",
                this->control_mode_ == ControlMode::CONTROL_MODE_CLOSED_LOOP ? LOG_STR_LITERAL("closed loop")
                                                                             : LOG_STR_LITERAL("open loop"));
  ESP_LOGCONFIG(TAG, "  Direction: %s",
                this->initial_direction_ == Direction::DIRECTION_BACKWARD ? LOG_STR_LITERAL("backward")
                                                                          : LOG_STR_LITERAL("forward"));
  ESP_LOGCONFIG(TAG, "  Motor model: %s",
                this->motor_model_ == MotorModel::MOTOR_MODEL_HIGH_SPEED ? LOG_STR_LITERAL("high speed")
                                                                         : LOG_STR_LITERAL("low speed"));
  ESP_LOGCONFIG(TAG, "  Pole pairs: %u", this->pole_pairs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with M5Unit-BLDC failed");
  }
}

bool M5UnitBldc::write_pwm_raw_(uint16_t duty) {
  uint8_t buf[2];
  std::memcpy(buf, &duty, 2);
  return this->write_register(REG_PWM, buf, 2) == i2c::ERROR_OK;
}

bool M5UnitBldc::read_pwm_raw_(uint16_t *duty) {
  uint8_t buf[2];
  if (this->read_register(REG_PWM, buf, 2) != i2c::ERROR_OK)
    return false;
  std::memcpy(duty, buf, 2);
  return true;
}

bool M5UnitBldc::write_pwm(uint16_t duty) {
  this->last_pwm_ = duty;
  if (this->direction_change_pending_)
    return true;
  return this->write_pwm_raw_(duty);
}

bool M5UnitBldc::write_target_rpm(float rpm) {
  this->last_target_rpm_ = rpm;
  if (this->direction_change_pending_)
    return true;
  return this->write_float_(REG_SET_RPM, rpm);
}

bool M5UnitBldc::write_direction(Direction direction) {
  uint8_t value = static_cast<uint8_t>(direction);
  if (this->write_register(REG_DIRECTION, &value, 1) != i2c::ERROR_OK)
    return false;

  // The device only latches a new direction once the motor has actually spun down to a stop --
  // zeroing and immediately restoring the PWM/RPM register isn't enough, it needs real time to
  // decelerate first (datasheet note "[1] Change direction": setup direction, zero, then
  // non-zero once stopped). 800ms comfortably covers spin-down for the small motors this driver
  // targets; restoring is deferred via a named timeout so repeated direction changes replace any
  // still-pending restore instead of stacking. While a reversal is pending, `write_pwm()` and
  // `write_target_rpm()` only cache the latest request instead of writing it, so a command
  // arriving mid-cycle cannot make the motor resume before the stop interval ends.
  this->direction_change_pending_ = true;
  bool zeroed = this->control_mode_ == ControlMode::CONTROL_MODE_CLOSED_LOOP ? this->write_float_(REG_SET_RPM, 0.0f)
                                                                             : this->write_pwm_raw_(0);
  if (!zeroed) {
    // The motor was never actually told to stop, so the 800ms spin-down assumption below does not
    // hold; abandon the reversal instead of restoring a command to a motor that may still be
    // running at the old direction.
    this->direction_change_pending_ = false;
    return false;
  }

  this->set_timeout(DIRECTION_CHANGE_TIMEOUT_ID, 800, [this]() {
    this->direction_change_pending_ = false;
    if (this->control_mode_ == ControlMode::CONTROL_MODE_CLOSED_LOOP) {
      if (this->last_target_rpm_ != 0.0f)
        this->write_float_(REG_SET_RPM, this->last_target_rpm_);
    } else {
      if (this->last_pwm_ != 0)
        this->write_pwm_raw_(this->last_pwm_);
    }
  });
  return true;
}

}  // namespace esphome::m5_unit_bldc
