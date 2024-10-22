#pragma once

#include "esphome/components/switch/switch.h"
#include "../kt0803.h"

namespace esphome {
namespace kt0803 {

class AlcEnableSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_alc_enable(value);
  }
};

class AuEnhanceSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_au_enhance(value);
  }
};

class AutoPaDownSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_auto_pa_down(value);
  }
};

class MonoSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_mono(value);
  }
};

class MuteSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_mute(value);
  }
};

class PaBiasSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_pa_bias(value);
  }
};

class PaDownSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_pa_down(value);
  }
};

class RefClkEnableSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_ref_clk_enable(value);
  }
};

class SilenceDetectionSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_silence_detection(value);
  }
};

class StandbyEnableSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_standby_enable(value);
  }
};

class XtalEnableSwitch : public switch_::Switch, public Parented<KT0803Component> {
 protected:
  void write_state(bool value) override {
    this->publish_state(value);
    this->parent_->set_xtal_enable(value);
  }
};

}  // namespace kt0803
}  // namespace esphome
