#include "m5_unit_bldc.h"

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::m5_unit_bldc {

static const char *const TAG = "m5_unit_bldc";

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
    this->mark_failed();
    return;
  }

  uint8_t direction = static_cast<uint8_t>(this->initial_direction_);
  this->write_register(REG_DIRECTION, &direction, 1);

  uint8_t motor_config[2] = {static_cast<uint8_t>(this->motor_model_), this->pole_pairs_};
  this->write_register(REG_MOTOR_CONFIG, motor_config, 2);

  if (this->has_pid_) {
    uint8_t pid[12];
    int32_t p_int = static_cast<int32_t>(this->p_ * 100);
    int32_t i_int = static_cast<int32_t>(this->i_ * 100);
    int32_t d_int = static_cast<int32_t>(this->d_ * 100);
    std::memcpy(pid, &p_int, 4);
    std::memcpy(pid + 4, &i_int, 4);
    std::memcpy(pid + 8, &d_int, 4);
    this->write_register(REG_PID, pid, 12);
  }

  if (this->save_to_flash_) {
    uint8_t save = 1;
    this->write_register(REG_SAVE_TO_FLASH, &save, 1);
  }
}

void M5UnitBldc::update() {
  if (this->rpm_sensor_ != nullptr) {
    float rpm;
    if (this->read_float_(REG_READBACK_RPM, &rpm))
      this->rpm_sensor_->publish_state(rpm);
  }

  if (this->frequency_sensor_ != nullptr) {
    float freq;
    if (this->read_float_(REG_READBACK_FREQ, &freq))
      this->frequency_sensor_->publish_state(freq);
  }

  if (this->status_text_sensor_ != nullptr) {
    uint8_t status;
    if (this->read_register(REG_MOTOR_STATUS, &status, 1) == i2c::ERROR_OK) {
      const char *status_str;
      switch (static_cast<MotorStatus>(status)) {
        case MotorStatus::STANDBY:
          status_str = "Standby";
          break;
        case MotorStatus::RUNNING:
          status_str = "Running";
          break;
        case MotorStatus::ERROR:
          status_str = "Error";
          break;
        default:
          status_str = "Unknown";
          break;
      }
      this->status_text_sensor_->publish_state(status_str);
    }
  }
}

void M5UnitBldc::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Unit-BLDC:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Control mode: %s",
                this->control_mode_ == ControlMode::CLOSED_LOOP ? "closed loop" : "open loop");
  ESP_LOGCONFIG(TAG, "  Direction: %s", this->initial_direction_ == Direction::BACKWARD ? "backward" : "forward");
  ESP_LOGCONFIG(TAG, "  Motor model: %s", this->motor_model_ == MotorModel::HIGH_SPEED ? "high speed" : "low speed");
  ESP_LOGCONFIG(TAG, "  Pole pairs: %u", this->pole_pairs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with M5Unit-BLDC failed");
  }
}

void M5UnitBldc::write_pwm_raw_(uint16_t duty) {
  uint8_t buf[2];
  std::memcpy(buf, &duty, 2);
  this->write_register(REG_PWM, buf, 2);
}

void M5UnitBldc::write_pwm(uint16_t duty) {
  this->last_pwm_ = duty;
  this->write_pwm_raw_(duty);
}

void M5UnitBldc::write_target_rpm(float rpm) {
  this->last_target_rpm_ = rpm;
  this->write_float_(REG_SET_RPM, rpm);
}

void M5UnitBldc::write_direction(Direction direction) {
  uint8_t value = static_cast<uint8_t>(direction);
  this->write_register(REG_DIRECTION, &value, 1);

  // The device only latches a new direction once the motor has actually spun down to a stop --
  // zeroing and immediately restoring the PWM/RPM register isn't enough, it needs real time to
  // decelerate first (datasheet note "[1] Change direction": setup direction, zero, then
  // non-zero once stopped). 800ms comfortably covers spin-down for the small motors this driver
  // targets; restoring is deferred via a named timeout so repeated direction changes replace any
  // still-pending restore instead of stacking.
  if (this->control_mode_ == ControlMode::CLOSED_LOOP) {
    this->write_float_(REG_SET_RPM, 0.0f);
    if (this->last_target_rpm_ != 0.0f) {
      this->set_timeout("direction_change", 800, [this]() { this->write_float_(REG_SET_RPM, this->last_target_rpm_); });
    }
  } else {
    this->write_pwm_raw_(0);
    if (this->last_pwm_ != 0) {
      this->set_timeout("direction_change", 800, [this]() { this->write_pwm_raw_(this->last_pwm_); });
    }
  }
}

}  // namespace esphome::m5_unit_bldc
