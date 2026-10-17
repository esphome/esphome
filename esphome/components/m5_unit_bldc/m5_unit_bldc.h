#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::m5_unit_bldc {

// I2C register map, from M5Stack's "Unit BLDC I2C Protocol" datasheet and the official
// M5UnitBLDC Arduino library (https://github.com/m5stack/M5Unit-BLDC).
enum : uint8_t {
  REG_MODE = 0x00,           // 1B, W/R -- 0: open loop, 1: closed loop
  REG_PWM = 0x10,            // 2B, W/R -- little-endian uint16_t, 0-2047
  REG_READBACK_RPM = 0x20,   // 4B, R   -- float
  REG_READBACK_FREQ = 0x30,  // 4B, R   -- float, Hz
  REG_SET_RPM = 0x40,        // 4B, W/R -- float, only honoured in closed loop
  REG_PID = 0x50,            // 12B, W/R -- 3x int32_t, each value * 100 (P, I, D)
  REG_DIRECTION = 0x60,      // 1B, W/R -- 0: forward, 1: backward
  REG_MOTOR_CONFIG = 0x70,   // 2B, W/R -- byte0: motor model, byte1: pole pairs
  REG_MOTOR_STATUS = 0x80,   // 1B, R   -- 0: standby, 1: running, 2: error
  REG_SAVE_TO_FLASH = 0xF0,  // 1B, W   -- write 1 to persist model/pole pairs/PID
  REG_FIRMWARE_VERSION = 0xFE,
};

enum class ControlMode : uint8_t { CONTROL_MODE_OPEN_LOOP = 0, CONTROL_MODE_CLOSED_LOOP = 1 };
enum class Direction : uint8_t { DIRECTION_FORWARD = 0, DIRECTION_BACKWARD = 1 };
enum class MotorModel : uint8_t { MOTOR_MODEL_LOW_SPEED = 0, MOTOR_MODEL_HIGH_SPEED = 1 };
enum class MotorStatus : uint8_t { MOTOR_STATUS_STANDBY = 0, MOTOR_STATUS_RUNNING = 1, MOTOR_STATUS_ERROR = 2 };

/// Hub component for a single M5Stack M5Unit-BLDC brushless DC motor driver. Add `number`/`select`
/// sub-platforms to drive it, and `sensor`/`text_sensor` sub-platforms to read it back.
class M5UnitBldc : public i2c::I2CDevice, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_control_mode(ControlMode mode) { this->control_mode_ = mode; }
  void set_initial_direction(Direction direction) { this->initial_direction_ = direction; }
  void set_motor_model(MotorModel model) { this->motor_model_ = model; }
  void set_pole_pairs(uint8_t pole_pairs) { this->pole_pairs_ = pole_pairs; }
  void set_pid(float p, float i, float d) {
    this->has_pid_ = true;
    this->p_ = p;
    this->i_ = i;
    this->d_ = d;
  }
  void set_save_to_flash(bool save_to_flash) { this->save_to_flash_ = save_to_flash; }

  void set_rpm_sensor(sensor::Sensor *rpm_sensor) { this->rpm_sensor_ = rpm_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
  void set_status_text_sensor(text_sensor::TextSensor *status_text_sensor) {
    this->status_text_sensor_ = status_text_sensor;
  }
  void set_direction_select(select::Select *direction_select) { this->direction_select_ = direction_select; }
  void set_pwm_number(number::Number *pwm_number) { this->pwm_number_ = pwm_number; }
  void set_target_rpm_number(number::Number *target_rpm_number) { this->target_rpm_number_ = target_rpm_number; }

  // Called by the `number`/`select` sub-platforms to drive the motor. Return whether the I2C
  // write that defines the new state succeeded, so callers can skip publishing an entity state
  // that the device never actually accepted.
  bool write_pwm(uint16_t duty);
  bool write_target_rpm(float rpm);
  bool write_direction(Direction direction);

 protected:
  static constexpr uint32_t DIRECTION_CHANGE_TIMEOUT_ID = 0;

  bool write_float_(uint8_t reg, float value);
  bool read_float_(uint8_t reg, float *value);
  bool write_pwm_raw_(uint16_t duty);
  bool read_pwm_raw_(uint16_t *duty);

  ControlMode control_mode_{ControlMode::CONTROL_MODE_OPEN_LOOP};
  Direction initial_direction_{Direction::DIRECTION_FORWARD};
  MotorModel motor_model_{MotorModel::MOTOR_MODEL_LOW_SPEED};
  uint8_t pole_pairs_{1};
  bool has_pid_{false};
  float p_{0}, i_{0}, d_{0};
  bool save_to_flash_{false};

  // Last commanded PWM/target RPM, so `write_direction()` can restore them after the
  // zero-then-nonzero cycle the device needs to actually latch a new direction (see datasheet
  // note "[1] Change direction").
  uint16_t last_pwm_{0};
  float last_target_rpm_{0};

  // Set for the duration of the zero-then-restore cycle in `write_direction()`. While set, new
  // PWM/RPM commands only update `last_pwm_`/`last_target_rpm_`; they are not written to the
  // device until the pending restore fires, so a command arriving mid-cycle cannot make the motor
  // resume before it has actually stopped and the new direction has latched.
  bool direction_change_pending_{false};

  sensor::Sensor *rpm_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};
  select::Select *direction_select_{nullptr};
  number::Number *pwm_number_{nullptr};
  number::Number *target_rpm_number_{nullptr};
};

}  // namespace esphome::m5_unit_bldc
