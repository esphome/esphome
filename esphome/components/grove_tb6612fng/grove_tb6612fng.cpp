#include "grove_tb6612fng.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace grove_tb6612fng {

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
  if (!this->standby()) {
    this->mark_failed();
    return;
  }
}

bool GroveMotorDriveTB6612FNG::standby() {
  uint8_t status = 0;
  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STANDBY, &status, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Set standby failed!");
    this->status_set_warning();
    return false;
  }
  return true;
}

bool GroveMotorDriveTB6612FNG::not_standby() {
  uint8_t status = 0;
  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_NOT_STANDBY, &status, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Set not standby failed!");
    this->status_set_warning();
    return false;
  }
  return true;
}

void GroveMotorDriveTB6612FNG::set_i2c_addr(uint8_t addr) {
  if (addr == 0x00 || addr >= 0x80) {
    return;
  }
  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_SET_ADDR, &addr, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Set new i2c address failed!");
    this->status_set_warning();
    return;
  }
  this->set_i2c_address(addr);
}

void GroveMotorDriveTB6612FNG::dc_motor_run(uint8_t channel, int16_t speed) {
  speed = clamp<int16_t>(speed, -255, 255);

  buffer_[0] = channel;
  if (speed >= 0) {
    buffer_[1] = speed;
  } else {
    buffer_[1] = (uint8_t) (-speed);
  }

  if (speed >= 0) {
    if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_CW, buffer_, 2) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Run motor failed!");
      this->status_set_warning();
      return;
    }
  } else {
    if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_CCW, buffer_, 2) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Run motor failed!");
      this->status_set_warning();
      return;
    }
  }
}

void GroveMotorDriveTB6612FNG::dc_motor_brake(uint8_t channel) {
  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_BRAKE, &channel, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Break motor failed!");
    this->status_set_warning();
    return;
  }
}

void GroveMotorDriveTB6612FNG::dc_motor_stop(uint8_t channel) {
  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STOP, &channel, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Stop dc motor failed!");
    this->status_set_warning();
    return;
  }
}

void GroveMotorDriveTB6612FNG::stepper_run(StepperModeTypeT mode, int16_t steps, uint16_t rpm) {
  uint8_t cw = 0;
  // 0.1ms_per_step
  uint16_t ms_per_step = 0;

  if (steps > 0) {
    cw = 1;
  }
  // stop
  else if (steps == 0) {
    this->stepper_stop();
    return;
  } else if (steps == INT16_MIN) {
    steps = INT16_MAX;
  } else {
    steps = -steps;
  }

  rpm = clamp<uint16_t>(rpm, 1, 300);

  ms_per_step = (uint16_t) (3000.0 / (float) rpm);
  buffer_[0] = mode;
  buffer_[1] = cw;  //(cw=1) => cw; (cw=0) => ccw
  buffer_[2] = steps;
  buffer_[3] = (steps >> 8);
  buffer_[4] = ms_per_step;
  buffer_[5] = (ms_per_step >> 8);

  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_RUN, buffer_, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Run stepper failed!");
    this->status_set_warning();
    return;
  }
}

void GroveMotorDriveTB6612FNG::stepper_stop() {
  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_STOP, nullptr, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Send stop stepper failed!");
    this->status_set_warning();
    return;
  }
}

void GroveMotorDriveTB6612FNG::stepper_keep_run(StepperModeTypeT mode, uint16_t rpm, bool is_cw) {
  // 4=>infinite ccw  5=>infinite cw
  uint8_t cw = (is_cw) ? 5 : 4;
  // 0.1ms_per_step
  uint16_t ms_per_step = 0;

  rpm = clamp<uint16_t>(rpm, 1, 300);
  ms_per_step = (uint16_t) (3000.0 / (float) rpm);

  buffer_[0] = mode;
  buffer_[1] = cw;  //(cw=1) => cw; (cw=0) => ccw
  buffer_[2] = ms_per_step;
  buffer_[3] = (ms_per_step >> 8);

  if (this->write_register(GROVE_MOTOR_DRIVER_I2C_CMD_STEPPER_KEEP_RUN, buffer_, 4) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Write stepper keep run failed");
    this->status_set_warning();
    return;
  }
}
}  // namespace grove_tb6612fng
}  // namespace esphome
