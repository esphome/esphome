#pragma once

#include "esphome/components/switch/switch.h"
#include "../dsp408.h"

namespace esphome {
namespace dsp408 {

enum class SwitchKind : uint8_t {
  MASTER_MUTE = 0,
  CHANNEL_MUTE,
  CHANNEL_POLAR,
  INPUT_POLAR,
};

class DSP408Switch : public switch_::Switch, public Component {
 public:
  void set_parent(DSP408 *parent) { this->parent_ = parent; }
  void set_master_mute() { this->kind_ = SwitchKind::MASTER_MUTE; }
  void set_channel_mute(uint8_t ch) {
    this->kind_ = SwitchKind::CHANNEL_MUTE;
    this->channel_ = ch;
  }
  void set_channel_polar(uint8_t ch) {
    this->kind_ = SwitchKind::CHANNEL_POLAR;
    this->channel_ = ch;
  }
  void set_input_polar(uint8_t input_ch) {
    this->kind_ = SwitchKind::INPUT_POLAR;
    this->channel_ = input_ch;
  }

  void setup() override {}
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  DSP408 *parent_{nullptr};
  SwitchKind kind_{SwitchKind::MASTER_MUTE};
  uint8_t channel_{0};
};

}  // namespace dsp408
}  // namespace esphome
