#pragma once

#include "esphome/components/switch/switch.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class AcompEnableSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_acomp_enable(value);
  }
};

class AsqIalhSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_asq_ialh(value);
  }
};

class AsqIallSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_asq_iall(value);
  }
};

class AsqOvermodSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_asq_overmod(value);
  }
};

class LimiterEnableSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_limiter_enable(value);
  }
};

class MonoSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_mono(value);
  }
};

class MuteSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_mute(value);
  }
};

class OutputGpioSwitch : public switch_::Switch, public Parented<Si4713Component> {
 public:
  void set_pin(uint8_t pin) { this->pin_ = pin - 1; }

 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_output_gpio(this->pin_, value);
  }

  uint8_t pin_{0};
};

class PilotEnableSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_pilot_enable(value);
  }
};

class RDSEnableSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_rds_enable(value);
  }
};

class TunerEnableSwitch : public switch_::Switch, public Parented<Si4713Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_tuner_enable(value);
  }
};

}  // namespace si4713
}  // namespace esphome
