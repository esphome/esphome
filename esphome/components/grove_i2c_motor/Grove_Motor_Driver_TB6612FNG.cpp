#include "Grove_Motor_Driver_TB6612FNG.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace grove_motor_drive_TB6612FNG {

static const char *const TAG = "GroveMotorDriveTB6612FNG";

static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_BRAKE = 0x00;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_STOP = 0x01;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_CW = 0x02;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_CCW = 0x03;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_STANDBY = 0x04;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_NOT_STANDBY = 0x05;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_RUN = 0x06;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_STOP = 0x07;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_KEEP_RUN = 0x08;
static const uint8_t GROVE_MOTOR_DRIVER_I2C_CMD_SET_ADDR = 0x11;

void GroveMotorDriveTB6612FNG::dump_config() {
  ESP_LOGCONFIG(TAG, "GroveMotorDriveTB6612FNG:");
  LOG_I2C_DEVICE(this);
}

void GroveMotorDriveTB6612FNG::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Grove Motor Drive TB6612FNG ...");

  // this->standby();
}

void GroveMotorDriveTB6612FNG::standby() {
  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STANDBY, 0, 1);
  delayMicroseconds(1000);
}

void GroveMotorDriveTB6612FNG::notStandby() {
  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_NOT_STANDBY, 0, 1);
  delayMicroseconds(1000);
}

void GroveMotorDriveTB6612FNG::setI2cAddr(uint8_t addr) {
  if (addr == 0x00) {
    return;
  } else if (addr >= 0x80) {
    return;
  }
  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_SET_ADDR, &addr, 1);
  delayMicroseconds(100000);
  this->set_i2c_address(addr);
}

void GroveMotorDriveTB6612FNG::dcMotorRun(uint8_t chl, int16_t speed) {
  if (speed > 255) {
    speed = 255;
  } else if (speed < -255) {
    speed = -255;
  }

  _buffer[0] = chl;
  if (speed >= 0) {
    _buffer[1] = speed;
  } else {
    _buffer[1] = (uint8_t) (-speed);
  }

  if (speed >= 0) {
    this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_CW, _buffer, 2);
  } else {
    this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_CCW, _buffer, 2);
  }
  delayMicroseconds(100000);
}

void GroveMotorDriveTB6612FNG::dcMotorBrake(uint8_t chl) {
  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_BRAKE, &chl, 1);

  delayMicroseconds(100000);
}

void GroveMotorDriveTB6612FNG::dcMotorStop(uint8_t chl) {
  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STOP, &chl, 1);

  delayMicroseconds(100000);
}

void GroveMotorDriveTB6612FNG::stepperRun(stepper_mode_type_t mode, int16_t steps, uint16_t rpm) {
  uint8_t cw = 0;
  // 0.1ms_per_step
  uint16_t ms_per_step = 0;

  if (steps > 0) {
    cw = 1;
  }
  // stop
  else if (steps == 0) {
    stepperStop();
    return;
  } else if (steps == -32768) {
    steps = 32767;
  } else {
    steps = -steps;
  }

  if (rpm < 1) {
    rpm = 1;
  } else if (rpm > 300) {
    rpm = 300;
  }

  ms_per_step = (uint16_t) (3000.0 / (float) rpm);
  _buffer[0] = mode;
  _buffer[1] = cw;  //(cw=1) => cw; (cw=0) => ccw
  _buffer[2] = steps;
  _buffer[3] = (steps >> 8);
  _buffer[4] = ms_per_step;
  _buffer[5] = (ms_per_step >> 8);

  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_RUN, _buffer, 1);
  delayMicroseconds(100000);
}

void GroveMotorDriveTB6612FNG::stepperStop() {
  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_STOP, 0, 1);
  delayMicroseconds(100000);
}

void GroveMotorDriveTB6612FNG::stepperKeepRun(stepper_mode_type_t mode, uint16_t rpm, bool is_cw) {
  // 4=>infinite ccw  5=>infinite cw
  uint8_t cw = (is_cw) ? 5 : 4;
  // 0.1ms_per_step
  uint16_t ms_per_step = 0;

  if (rpm < 1) {
    rpm = 1;
  } else if (rpm > 300) {
    rpm = 300;
  }
  ms_per_step = (uint16_t) (3000.0 / (float) rpm);

  _buffer[0] = mode;
  _buffer[1] = cw;  //(cw=1) => cw; (cw=0) => ccw
  _buffer[2] = ms_per_step;
  _buffer[3] = (ms_per_step >> 8);

  this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_KEEP_RUN, _buffer, 4);
  delayMicroseconds(100000);
}
}  // namespace grove_motor_drive_TB6612FNG
}  // namespace esphome