#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
//#include "esphome/core/helpers.h"

/*
    Grove_Motor_Driver_TB6612FNG.h
    A library for the Grove - Motor Driver(TB6612FNG)
    Copyright (c) 2018 seeed technology co., ltd.
    Website    : www.seeed.cc
    Author     : Jerry Yip
    Create Time: 2018-06
    Version    : 0.1
    Change Log :
    The MIT License (MIT)
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

namespace esphome {
namespace grove_tb6612fng {

enum MotorChannelTypeT {
  MOTOR_CHA = 0,
  MOTOR_CHB = 1,
};

enum StepperModeTypeT {
  FULL_STEP = 0,
  WAVE_DRIVE = 1,
  HALF_STEP = 2,
  MICRO_STEPPING = 3,
};

class GroveMotorDriveTB6612FNG : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  /*************************************************************
      Description
       Enter standby mode. Normally you don't need to call this, except that
       you have called notStandby() before.
      Parameter
       Null.
      Return
       True/False.
  *************************************************************/
  bool standby();

  /*************************************************************
      Description
       Exit standby mode. Motor driver does't do any action at this mode.
      Parameter
       Null.
      Return
       True/False.
  *************************************************************/
  bool not_standby();

  /*************************************************************
      Description
       Set an new I2C address.
      Parameter
       addr: 0x01~0x7f
      Return
       Null.
  *************************************************************/
  void set_i2c_addr(uint8_t addr);

  /***********************************change_address
       Drive a motor.
      Parameter
       chl: MOTOR_CHA or MOTOR_CHB
       speed: -255~255, if speed > 0, motor moves clockwise.
              Note that there is always a starting speed(a starting voltage) for motor.
              If the input voltage is 5V, the starting speed should larger than 100 or
              smaller than -100.
      Return
       Null.
  *************************************************************/
  void dc_motor_run(uint8_t channel, int16_t speed);

  /*************************************************************
      Description
       Brake, stop the motor immediately
      Parameter
       chl: MOTOR_CHA or MOTOR_CHB
      Return
       Null.
  *************************************************************/
  void dc_motor_brake(uint8_t channel);

  /*************************************************************
      Description
       Stop the motor slowly.
      Parameter
       chl: MOTOR_CHA or MOTOR_CHB
      Return
       Null.
  *************************************************************/
  void dc_motor_stop(uint8_t channel);

  /*************************************************************
      Description
       Drive a stepper.
      Parameter
       mode:  4 driver mode: FULL_STEP,WAVE_DRIVE, HALF_STEP, MICRO_STEPPING,
              for more information: https://en.wikipedia.org/wiki/Stepper_motor#/media/File:Drive.png
       steps: The number of steps to run, range from -32768 to 32767.
              When steps = 0, the stepper stops.
              When steps > 0, the stepper runs clockwise. When steps < 0, the stepper runs anticlockwise.
       rpm:   Revolutions per minute, the speed of a stepper, range from 1 to 300.
              Note that high rpm will lead to step lose, so rpm should not be larger than 150.
      Return
       Null.
  *************************************************************/
  void stepper_run(StepperModeTypeT mode, int16_t steps, uint16_t rpm);

  /*************************************************************
      Description
       Stop a stepper.
      Parameter
       Null.
      Return
       Null.
  *************************************************************/
  void stepper_stop();

  // keeps moving(direction same as the last move, default to clockwise)
  /*************************************************************
      Description
       Keep a stepper running.
      Parameter
       mode:  4 driver mode: FULL_STEP,WAVE_DRIVE, HALF_STEP, MICRO_STEPPING,
              for more information: https://en.wikipedia.org/wiki/Stepper_motor#/media/File:Drive.png
       rpm:   Revolutions per minute, the speed of a stepper, range from 1 to 300.
              Note that high rpm will lead to step lose, so rpm should not be larger than 150.
       is_cw: Set the running direction, true for clockwise and false for anti-clockwise.
      Return
       Null.
  *************************************************************/
  void stepper_keep_run(StepperModeTypeT mode, uint16_t rpm, bool is_cw);

 private:
  uint8_t buffer_[16];
};

template<typename... Ts>
class GROVETB6612FNGMotorRunAction : public Action<Ts...>, public Parented<GroveMotorDriveTB6612FNG> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(uint16_t, speed)

  void play(Ts... x) override {
    auto channel = this->channel_.value(x...);
    auto speed = this->speed_.value(x...);
    this->parent_->dc_motor_run(channel, speed);
  }
};

template<typename... Ts>
class GROVETB6612FNGMotorBrakeAction : public Action<Ts...>, public Parented<GroveMotorDriveTB6612FNG> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)

  void play(Ts... x) override { this->parent_->dc_motor_brake(this->channel_.value(x...)); }
};

template<typename... Ts>
class GROVETB6612FNGMotorStopAction : public Action<Ts...>, public Parented<GroveMotorDriveTB6612FNG> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)

  void play(Ts... x) override { this->parent_->dc_motor_stop(this->channel_.value(x...)); }
};

template<typename... Ts>
class GROVETB6612FNGMotorStandbyAction : public Action<Ts...>, public Parented<GroveMotorDriveTB6612FNG> {
 public:
  void play(Ts... x) override { this->parent_->standby(); }
};

template<typename... Ts>
class GROVETB6612FNGMotorNoStandbyAction : public Action<Ts...>, public Parented<GroveMotorDriveTB6612FNG> {
 public:
  void play(Ts... x) override { this->parent_->not_standby(); }
};

template<typename... Ts>
class GROVETB6612FNGMotorChangeAddressAction : public Action<Ts...>, public Parented<GroveMotorDriveTB6612FNG> {
 public:
  TEMPLATABLE_VALUE(uint8_t, address)

  void play(Ts... x) override { this->parent_->set_i2c_addr(this->address_.value(x...)); }
};

}  // namespace grove_tb6612fng
}  // namespace esphome
