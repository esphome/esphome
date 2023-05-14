#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

#include "Grove_Motor_Driver_TB6612FNG.h"

// Remove the defined min function that conflicts with the framework
#ifdef min
#undef min
#endif

namespace esphome {
namespace grove_i2c_motor {

class GroveMotorDriveTB6612FNG : public Component {
 public:
  void setup() override;
  void dump_config() override;

  void standby();

  void not_standby();

  void set_i2c_addr(uint8_t addr);

  void dc_motor_run(uint8_t chl, int16_t speed);

  void dc_motor_brake(uint8_t chl);

  void dc_motor_stop(uint8_t chl);

  void stepper_run(int mode, int16_t steps, uint16_t rpm);

  void stepper_stop();

  void stepper_keep_run(int mode, uint16_t rpm, bool is_cw);
  void set_address(uint8_t addr) { this->address = addr; }

  MotorDriver *motor{nullptr};
  uint8_t address;
};

template<typename... Ts> class GROVETB6612FNGMotorRunAction : public Action<Ts...> {
 public:
  GROVETB6612FNGMotorRunAction(GroveMotorDriveTB6612FNG *motor) : motor_(motor) {}
  TEMPLATABLE_VALUE(uint8_t, chl)
  TEMPLATABLE_VALUE(uint16_t, speed)

  void play(Ts... x) override {
    auto chl = this->chl_.value(x...);
    auto speed = this->speed_.value(x...);
    this->motor_->dc_motor_run(chl, speed);
  }

 protected:
  GroveMotorDriveTB6612FNG *motor_;
};

template<typename... Ts> class GROVETB6612FNGMotorBrakeAction : public Action<Ts...> {
 public:
  GROVETB6612FNGMotorBrakeAction(GroveMotorDriveTB6612FNG *motor) : motor_(motor) {}
  TEMPLATABLE_VALUE(uint8_t, chl)

  void play(Ts... x) override { this->motor_->dc_motor_brake(this->chl_.value(x...)); }

 protected:
  GroveMotorDriveTB6612FNG *motor_;
};

template<typename... Ts> class GROVETB6612FNGMotorStopAction : public Action<Ts...> {
 public:
  GROVETB6612FNGMotorStopAction(GroveMotorDriveTB6612FNG *motor) : motor_(motor) {}
  TEMPLATABLE_VALUE(uint8_t, chl)

  void play(Ts... x) override { this->motor_->dc_motor_stop(this->chl_.value(x...)); }

 protected:
  GroveMotorDriveTB6612FNG *motor_;
};

template<typename... Ts> class GROVETB6612FNGMotorStandbyAction : public Action<Ts...> {
 public:
  GROVETB6612FNGMotorStandbyAction(GroveMotorDriveTB6612FNG *motor) : motor_(motor) {}

  void play(Ts... x) override { this->motor_->standby(); }

 protected:
  GroveMotorDriveTB6612FNG *motor_;
};

template<typename... Ts> class GROVETB6612FNGMotorNoStandbyAction : public Action<Ts...> {
 public:
  GROVETB6612FNGMotorNoStandbyAction(GroveMotorDriveTB6612FNG *motor) : motor_(motor) {}

  void play(Ts... x) override { this->motor_->not_standby(); }

 protected:
  GroveMotorDriveTB6612FNG *motor_;
};

}  // namespace grove_i2c_motor
}  // namespace esphome
#endif
