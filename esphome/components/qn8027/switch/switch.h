#pragma once

#include "esphome/components/switch/switch.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class MonoSwitch : public switch_::Switch, public Parented<QN8027Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_mono(value);
  }
};

class MuteSwitch : public switch_::Switch, public Parented<QN8027Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_mute(value);
  }
};

class PrivEnSwitch : public switch_::Switch, public Parented<QN8027Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_priv_en(value);
  }
};

class RDSEnableSwitch : public switch_::Switch, public Parented<QN8027Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_rds_enable(value);
  }
};

class TxEnableSwitch : public switch_::Switch, public Parented<QN8027Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_tx_enable(value);
  }
};

}  // namespace qn8027
}  // namespace esphome
