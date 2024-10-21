#pragma once

#include "esphome/components/number/number.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class DeviationNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_deviation(value);
  }
};

class DigitalGainNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_digital_gain((uint8_t) lround(value));
  }
};

class FrequencyNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_frequency(value);
  }
};

class InputGainNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_input_gain((uint8_t) lround(value));
  }
};

class PowerTargetNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_power_target(value);
  }
};

class RDSDeviationNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_rds_deviation(value);
  }
};

class TxPilotNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tx_pilot((uint8_t) lround(value));
  }
};

class XtalCurrentNumber : public number::Number, public Parented<QN8027Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_xtal_current(value);
  }
};

}  // namespace qn8027
}  // namespace esphome
