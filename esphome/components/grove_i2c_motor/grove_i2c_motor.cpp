#include "grove_i2c_motor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace grove_i2c_motor {

static const char *const TAG = "GroveMotorDriveTB6612FNG";

void GroveMotorDriveTB6612FNG::dump_config() {
  ESP_LOGCONFIG(TAG, "GroveMotorDriveTB6612FNG:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address);
}

void GroveMotorDriveTB6612FNG::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Grove Motor Drive TB6612FNG ...");

  motor->init(this->address);
}

void GroveMotorDriveTB6612FNG::standby() { motor->standby(); }

void GroveMotorDriveTB6612FNG::not_standby() { motor->notStandby(); }

void GroveMotorDriveTB6612FNG::set_i2c_addr(uint8_t addr) { motor->setI2cAddr(addr); }

void GroveMotorDriveTB6612FNG::dc_motor_run(uint8_t chl, int16_t speed) {
  motor_channel_type_t channel;
  if (chl == 0) {
    channel = MOTOR_CHA;
  } else {
    channel = MOTOR_CHB;
  }
  motor->dcMotorRun(channel, speed);
}

void GroveMotorDriveTB6612FNG::dc_motor_brake(uint8_t chl) {
  motor_channel_type_t channel;
  if (chl == 0) {
    channel = MOTOR_CHA;
  } else {
    channel = MOTOR_CHB;
  }
  motor->dcMotorBrake(channel);
}

void GroveMotorDriveTB6612FNG::dc_motor_stop(uint8_t chl) {
  motor_channel_type_t channel;
  if (chl == 0) {
    channel = MOTOR_CHA;
  } else {
    channel = MOTOR_CHB;
  }
  motor->dcMotorStop(channel);
}

void GroveMotorDriveTB6612FNG::stepper_run(int mode, int16_t steps, uint16_t rpm) {
  stepper_mode_type_t mode_type;

  if (mode == 0) {
    mode_type = FULL_STEP;
  } else if (mode == 1) {
    mode_type = WAVE_DRIVE;
  } else if (mode == 2) {
    mode_type = HALF_STEP;
  } else {
    mode_type = MICRO_STEPPING;
  }
  motor->stepperRun(mode_type, steps, rpm);
}

void GroveMotorDriveTB6612FNG::stepper_stop() { motor->stepperStop(); }

void GroveMotorDriveTB6612FNG::stepper_keep_run(int mode, uint16_t rpm, bool is_cw) {
  stepper_mode_type_t mode_type;

  if (mode == 0) {
    mode_type = FULL_STEP;
  } else if (mode == 1) {
    mode_type = WAVE_DRIVE;
  } else if (mode == 2) {
    mode_type = HALF_STEP;
  } else {
    mode_type = MICRO_STEPPING;
  }
  motor->stepperKeepRun(mode_type, rpm, is_cw);
}

}  // namespace grove_i2c_motor
}  // namespace esphome
