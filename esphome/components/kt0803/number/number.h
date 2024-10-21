#pragma once

#include "esphome/components/number/number.h"
#include "../kt0803.h"

namespace esphome {
namespace kt0803 {

class AlcGainNumber : public number::Number, public Parented<KT0803Component> {
 public:
  AlcGainNumber() = default;

 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_alc_gain(value);
  }
};

class FrequencyNumber : public number::Number, public Parented<KT0803Component> {
 public:
  FrequencyNumber() = default;

 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_frequency(value);
  }
};

class PgaNumber : public number::Number, public Parented<KT0803Component> {
 public:
  PgaNumber() = default;

 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_pga(value);
  }
};

class RfGainNumber : public number::Number, public Parented<KT0803Component> {
 public:
  RfGainNumber() = default;

 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_rfgain(value);
  }
};

}  // namespace kt0803
}  // namespace esphome
