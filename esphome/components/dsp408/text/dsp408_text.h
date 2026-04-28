#pragma once

#include "esphome/components/text/text.h"
#include "../dsp408.h"

namespace esphome {
namespace dsp408 {

enum class TextKind : uint8_t {
  PRESET_NAME = 0,
  CHANNEL_NAME,
};

class DSP408Text : public text::Text, public Component {
 public:
  void set_parent(DSP408 *parent) { this->parent_ = parent; }
  void set_preset_name() { this->kind_ = TextKind::PRESET_NAME; }
  void set_channel_name(uint8_t ch) {
    this->kind_ = TextKind::CHANNEL_NAME;
    this->channel_ = ch;
  }

  void setup() override {}
  void dump_config() override;

 protected:
  void control(const std::string &value) override;

  DSP408 *parent_{nullptr};
  TextKind kind_{TextKind::PRESET_NAME};
  uint8_t channel_{0};
};

}  // namespace dsp408
}  // namespace esphome
