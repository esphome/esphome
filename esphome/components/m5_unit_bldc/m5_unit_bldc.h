#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/i2c/i2c.h"
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::m5_unit_bldc {

// I2C register map, from M5Stack's "Unit BLDC I2C Protocol" datasheet and the M5UnitBLDC library
// (https://github.com/m5stack/M5Unit-BLDC). Multi-byte values are little-endian.
enum : uint8_t {
  REG_MODE = 0x00,           // 1B, 0: open loop, 1: closed loop
  REG_PWM = 0x10,            // 2B, uint16_t duty 0-2047
  REG_READBACK_RPM = 0x20,   // 4B, read only, float
  REG_READBACK_FREQ = 0x30,  // 4B, read only, float Hz
  REG_SET_RPM = 0x40,        // 4B, float, closed loop only
  REG_PID = 0x50,            // 12B, 3x int32_t, each gain * 100
  REG_DIRECTION = 0x60,      // 1B, 0: forward, 1: backward
  REG_MOTOR_CONFIG = 0x70,   // 2B, motor model, pole pairs
  REG_MOTOR_STATUS = 0x80,   // 1B, read only, 0: standby, 1: running, 2: error
  REG_SAVE_TO_FLASH = 0xF0,  // 1B, write 1 to persist model, pole pairs and PID
};

enum class ControlMode : uint8_t { CONTROL_MODE_OPEN_LOOP = 0, CONTROL_MODE_CLOSED_LOOP = 1 };
// Values match the direction select's option order
enum class Direction : uint8_t { DIRECTION_FORWARD = 0, DIRECTION_BACKWARD = 1 };
enum class MotorModel : uint8_t { MOTOR_MODEL_LOW_SPEED = 0, MOTOR_MODEL_HIGH_SPEED = 1 };

/// Hub for one M5Stack M5Unit-BLDC brushless motor driver.
class M5UnitBldc : public i2c::I2CDevice, public PollingComponent {
 public:
  M5UnitBldc(ControlMode mode, Direction initial_direction, MotorModel model, uint8_t pole_pairs, bool save_to_flash)
      : control_mode_(mode),
        initial_direction_(initial_direction),
        motor_model_(model),
        pole_pairs_(pole_pairs),
        save_to_flash_(save_to_flash) {}

  void setup() override;
  void update() override;
  void dump_config() override;

  /// PID gains as the device stores them, each multiplied by 100.
  void set_pid(int32_t p, int32_t i, int32_t d) {
    this->pid_[0] = p;
    this->pid_[1] = i;
    this->pid_[2] = d;
    this->has_pid_ = true;
  }

#ifdef USE_SENSOR
  void set_rpm_sensor(sensor::Sensor *rpm_sensor) { this->rpm_sensor_ = rpm_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_status_text_sensor(text_sensor::TextSensor *status_text_sensor) {
    this->status_text_sensor_ = status_text_sensor;
  }
#endif
#ifdef USE_NUMBER
  /// The PWM duty number in open loop, the target RPM number in closed loop.
  void set_command_number(number::Number *command_number) { this->command_number_ = command_number; }
  void control_command(float value) {
    if (this->write_command(value))
      this->command_number_->publish_state(value);
  }
#endif
#ifdef USE_SELECT
  void set_direction_select(select::Select *direction_select) { this->direction_select_ = direction_select; }
  void control_direction(size_t index) {
    if (this->write_direction(static_cast<Direction>(index)))
      this->direction_select_->publish_state(index);
  }
#endif

  /// Send the PWM duty (open loop) or target RPM (closed loop). False when refused or the write failed;
  /// commands are refused once setup failed, since the motor configuration may be incomplete.
  bool write_command(float value);
  bool write_direction(Direction direction);

 protected:
  static constexpr uint32_t DIRECTION_CHANGE_TIMEOUT_ID = 0;

  template<typename T> bool read_reg_(uint8_t reg, T *value) {
    return this->read_bytes(reg, reinterpret_cast<uint8_t *>(value), sizeof(T));
  }
  template<typename T> bool write_reg_(uint8_t reg, const T &value) {
    return this->write_bytes(reg, reinterpret_cast<const uint8_t *>(&value), sizeof(T));
  }
  bool write_command_raw_(float value);
  bool read_command_raw_(float *value);

#ifdef USE_SENSOR
  sensor::Sensor *rpm_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *status_text_sensor_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *direction_select_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *command_number_{nullptr};
#endif
  int32_t pid_[3]{};
  // Last command, restored after the stop a direction change needs
  float last_command_{0};
  const ControlMode control_mode_;
  const Direction initial_direction_;
  const MotorModel motor_model_;
  const uint8_t pole_pairs_;
  const bool save_to_flash_;
  bool has_pid_{false};
  // While a direction change waits for the motor to stop, commands are only cached
  bool direction_change_pending_{false};
#ifdef USE_TEXT_SENSOR
  uint8_t last_status_{0xFF};
#endif
};

}  // namespace esphome::m5_unit_bldc
