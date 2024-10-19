#pragma once

#include "esphome/components/select/select.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class AcompAttackSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_acomp_attack((AcompAttack) *index);
    }
  }
};

class AcompPresetSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_acomp_preset((AcompPreset) *index);
    }
  }
};

class AcompReleaseSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_acomp_release((AcompRelease) *index);
    }
  }
};

class AnalogAttenuationSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_analog_attenuation((LineAttenuation) *index);
    }
  }
};

class DigitalChannelsSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_digital_channels((SampleChannels) *index);
    }
  }
};


class DigitalClockEdgeSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_digital_clock_edge((DigitalClockEdge) *index);
    }
  }
};

class DigitalModeSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_digital_mode((DigitalMode) *index);
    }
  }
};

class DigitalSampleBitsSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_digital_sample_bits((SampleBits) *index);
    }
  }
};

class PreEmphasisSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_pre_emphasis((PreEmphasis) *index);
    }
  }
};

class RefClkSourceSelect : public select::Select, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_refclk_source((RefClkSource) *index);
    }
  }
};

}  // namespace si4713
}  // namespace esphome
