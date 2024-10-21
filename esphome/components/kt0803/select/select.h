#pragma once

#include "esphome/components/select/select.h"
#include "../kt0803.h"

namespace esphome {
namespace kt0803 {

class AlcAttackTimeSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_alc_attack_time((AlcTime) *index);
    }
  }
};

class AlcDecayTimeSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_alc_decay_time((AlcTime) *index);
    }
  }
};

class AlcHighSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_alc_high((AlcHigh) *index);
    }
  }
};

class AlcHoldTimeSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_alc_hold_time((AlcHoldTime) *index);
    }
  }
};

class AlcLowSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_alc_low((AlcLow) *index);
    }
  }
};

class AudioLimiterLevelSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_audio_limiter_level((AudioLimiterLevel) *index);
    }
  }
};

class BassBoostControlSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_bass_boost_control((BassBoostControl) *index);
    }
  }
};

class FrequencyDeviationSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_deviation((FrequencyDeviation) *index);
    }
  }
};

class PilotToneAmplitudeSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_pilot_tone_amplitude((PilotToneAmplitude) *index);
    }
  }
};

class PreEmphasisSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_pre_emphasis((PreEmphasis) *index);
    }
  }
};

class RefClkSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_ref_clk_sel((ReferenceClock) *index);
    }
  }
};

class SilenceDurationSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_silence_duration((SilenceLowAndHighLevelDurationTime) *index);
    }
  }
};

class SilenceHighCounterSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_silence_high_counter((SilenceHighLevelCounter) *index);
    }
  }
};

class SilenceHighSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_silence_high((SilenceHigh) *index);
    }
  }
};

class SilenceLowCounterSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_silence_low_counter((SilenceLowLevelCounter) *index);
    }
  }
};

class SilenceLowSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_silence_low((SilenceLow) *index);
    }
  }
};

class SwitchModeSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_switch_mode((SwitchMode) *index);
    }
  }
};

class XtalSelSelect : public select::Select, public Parented<KT0803Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    if (auto index = this->active_index()) {
      this->parent_->set_xtal_sel((XtalSel) *index);
    }
  }
};

}  // namespace kt0803
}  // namespace esphome
