#include "m5stack_unit_bldc.h"

#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::m5stack_unit_bldc {

ESPHOME_LOG_TAG(TAG, "m5stack_unit_bldc");

#ifdef USE_TEXT_SENSOR
// Indexed by the REG_MOTOR_STATUS byte; anything past "Error" reads as "Unknown"
PROGMEM_STRING_TABLE(MotorStatusStrings, "Standby", "Running", "Error", "Unknown");
#endif

// The device latches a new direction only once the motor has stopped (datasheet note [1] "Change direction")
static constexpr uint32_t DIRECTION_CHANGE_STOP_MS = 800;

bool M5StackUnitBldc::write_command_raw_(float value) {
  if (this->control_mode_ == ControlMode::CONTROL_MODE_CLOSED_LOOP)
    return this->write_reg_(REG_SET_RPM, value);
  return this->write_reg_(REG_PWM, static_cast<uint16_t>(value));
}

bool M5StackUnitBldc::read_command_raw_(float *value) {
  if (this->control_mode_ == ControlMode::CONTROL_MODE_CLOSED_LOOP)
    return this->read_reg_(REG_SET_RPM, value);
  uint16_t duty;
  if (!this->read_reg_(REG_PWM, &duty))
    return false;
  *value = duty;
  return true;
}

void M5StackUnitBldc::setup() {
  if (!this->write_byte(REG_MODE, static_cast<uint8_t>(this->control_mode_))) {
    this->mark_failed(LOG_STR("Failed to write control mode"));
    return;
  }
  // The unit keeps driving its last command across an ESP reboot, so the cache and the number start from it, and
  // a running motor only takes the direction through the stop and restore sequence
  float command;
  if (!this->read_command_raw_(&command)) {
    this->mark_failed(LOG_STR("Failed to read the motor command"));
    return;
  }
  this->last_command_ = command;
  // REG_DIRECTION is readable (datasheet register 0x60, W/R), so a boot that already matches the configured
  // direction skips the stop-and-restore cycle entirely instead of spinning the motor down for no reason. A
  // failed read is treated the same as a mismatch, falling back to the unconditional write below.
  uint8_t current_direction;
  bool direction_matches = this->read_byte(REG_DIRECTION, &current_direction) &&
                           current_direction == static_cast<uint8_t>(this->initial_direction_);
  if (!direction_matches &&
      (command != 0 ? !this->write_direction_raw_(this->initial_direction_)
                    : !this->write_byte(REG_DIRECTION, static_cast<uint8_t>(this->initial_direction_)))) {
    this->mark_failed(LOG_STR("Failed to write initial direction"));
    return;
  }
#ifdef USE_SELECT
  if (this->direction_select_ != nullptr)
    this->direction_select_->publish_state(static_cast<size_t>(this->initial_direction_));
#endif
#ifdef USE_NUMBER
  if (this->command_number_ != nullptr)
    this->command_number_->publish_state(command);
#endif

  // The previous values only decide whether to save, so they are read only when saving is enabled
  const uint8_t motor_config[2] = {static_cast<uint8_t>(this->motor_model_), this->pole_pairs_};
  bool changed = false;
  if (this->save_to_flash_) {
    uint8_t previous[2];
    changed = !this->read_reg_(REG_MOTOR_CONFIG, &previous) || std::memcmp(previous, motor_config, 2) != 0;
  }
  if (!this->write_reg_(REG_MOTOR_CONFIG, motor_config)) {
    this->mark_failed(LOG_STR("Failed to write motor model/pole pairs"));
    return;
  }

  if (this->has_pid_) {
    if (this->save_to_flash_ && !changed) {
      int32_t previous[3];
      changed = !this->read_reg_(REG_PID, &previous) || std::memcmp(previous, this->pid_, sizeof(previous)) != 0;
    }
    if (!this->write_reg_(REG_PID, this->pid_)) {
      this->mark_failed(LOG_STR("Failed to write PID gains"));
      return;
    }
  }

  // The unit's flash has limited write cycles, so only a change is saved. This compares the live registers, so a
  // value first applied with save_to_flash off is saved only after the unit itself is power-cycled.
  if (changed && !this->write_byte(REG_SAVE_TO_FLASH, 1)) {
    ESP_LOGW(TAG, "Failed to save settings to flash");
  }

  // Without a readback entity there is nothing to poll
#ifdef USE_SENSOR
  if (this->rpm_sensor_ != nullptr || this->frequency_sensor_ != nullptr)
    return;
#endif
#ifdef USE_TEXT_SENSOR
  if (this->status_text_sensor_ != nullptr)
    return;
#endif
  this->stop_poller();
}

void M5StackUnitBldc::update() {
  bool ok = true;
#ifdef USE_SENSOR
  float value;
  if (this->rpm_sensor_ != nullptr) {
    if (this->read_reg_(REG_READBACK_RPM, &value)) {
      this->rpm_sensor_->publish_state(value);
    } else {
      ok = false;
    }
  }
  if (this->frequency_sensor_ != nullptr) {
    if (this->read_reg_(REG_READBACK_FREQ, &value)) {
      this->frequency_sensor_->publish_state(value);
    } else {
      ok = false;
    }
  }
#endif
#ifdef USE_TEXT_SENSOR
  uint8_t status;
  if (this->status_text_sensor_ != nullptr) {
    if (!this->read_byte(REG_MOTOR_STATUS, &status)) {
      ok = false;
    } else if (status != this->last_status_) {
      this->last_status_ = status;
      char text[MotorStatusStrings::MAX_LENGTH + 1];
      ESPHOME_strncpy_P(
          text,
          reinterpret_cast<ESPHOME_PGM_P>(MotorStatusStrings::get_progmem_str(status, MotorStatusStrings::LAST_INDEX)),
          sizeof(text));
      this->status_text_sensor_->publish_state(text);
    }
  }
#endif
  this->status_comm_result(ok);
}

void M5StackUnitBldc::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5Unit-BLDC:\n"
                "  Control mode: %s\n"
                "  Direction: %s\n"
                "  Motor model: %s\n"
                "  Pole pairs: %u",
                this->control_mode_ == ControlMode::CONTROL_MODE_CLOSED_LOOP ? LOG_STR_LITERAL("closed loop")
                                                                             : LOG_STR_LITERAL("open loop"),
                this->initial_direction_ == Direction::DIRECTION_BACKWARD ? LOG_STR_LITERAL("backward")
                                                                          : LOG_STR_LITERAL("forward"),
                this->motor_model_ == MotorModel::MOTOR_MODEL_HIGH_SPEED ? LOG_STR_LITERAL("high speed")
                                                                         : LOG_STR_LITERAL("low speed"),
                this->pole_pairs_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

// Outside a direction change the cache only follows a write the device accepted, so a refused command is not
// applied later when the direction change restores the command
bool M5StackUnitBldc::write_command(float value) {
  if (this->is_failed())
    return false;
  if (!this->direction_change_pending_ && !this->status_comm_result(this->write_command_raw_(value)))
    return false;
  this->last_command_ = value;
  return true;
}

bool M5StackUnitBldc::write_direction(Direction direction) {
  return !this->is_failed() && this->status_comm_result(this->write_direction_raw_(direction));
}

// The new direction latches only after the motor stops: command zero, wait, then restore. A failed stop leaves
// any direction change already pending as it is.
bool M5StackUnitBldc::write_direction_raw_(Direction direction) {
  if (!this->write_byte(REG_DIRECTION, static_cast<uint8_t>(direction)) || !this->write_command_raw_(0))
    return false;
  this->direction_change_pending_ = true;
  this->set_timeout(DIRECTION_CHANGE_TIMEOUT_ID, DIRECTION_CHANGE_STOP_MS, [this]() {
    this->direction_change_pending_ = false;
    if (this->last_command_ != 0)
      this->status_comm_result(this->write_command_raw_(this->last_command_));
  });
  return true;
}

}  // namespace esphome::m5stack_unit_bldc
