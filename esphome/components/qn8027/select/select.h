#pragma once

#include "esphome/components/select/select.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class InputImpedanceSelect : public select::Select, public Parented<QN8027Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_input_impedance((InputImpedance) *index);
    }
  }
};

class PreEmphasisSelect : public select::Select, public Parented<QN8027Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_pre_emphasis((PreEmphasis) *index);
    }
  }
};

class T1mSelSelect : public select::Select, public Parented<QN8027Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_t1m_sel((T1mSel) *index);
    }
  }
};

class XtalFrequencySelect : public select::Select, public Parented<QN8027Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_xtal_frequency((XtalFrequency) *index);
    }
  }
};

class XtalSourceSelect : public select::Select, public Parented<QN8027Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_xtal_source((XtalSource) *index);
    }
  }
};
}  // namespace qn8027
}  // namespace esphome
