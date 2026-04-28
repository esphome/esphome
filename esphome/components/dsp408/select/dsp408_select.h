#pragma once

#include "esphome/components/select/select.h"
#include "../dsp408.h"

namespace esphome {
namespace dsp408 {

enum class SelectKind : uint8_t {
  CHANNEL_HPF_FILTER = 0,
  CHANNEL_HPF_SLOPE,
  CHANNEL_LPF_FILTER,
  CHANNEL_LPF_SLOPE,
};

class DSP408Select : public select::Select, public Component {
 public:
  void set_parent(DSP408 *parent) { this->parent_ = parent; }
  void set_channel_hpf_filter(uint8_t ch) {
    this->kind_ = SelectKind::CHANNEL_HPF_FILTER;
    this->channel_ = ch;
  }
  void set_channel_hpf_slope(uint8_t ch) {
    this->kind_ = SelectKind::CHANNEL_HPF_SLOPE;
    this->channel_ = ch;
  }
  void set_channel_lpf_filter(uint8_t ch) {
    this->kind_ = SelectKind::CHANNEL_LPF_FILTER;
    this->channel_ = ch;
  }
  void set_channel_lpf_slope(uint8_t ch) {
    this->kind_ = SelectKind::CHANNEL_LPF_SLOPE;
    this->channel_ = ch;
  }

  void setup() override {}
  void dump_config() override;

 protected:
  void control(const std::string &value) override;

  DSP408 *parent_{nullptr};
  SelectKind kind_{SelectKind::CHANNEL_HPF_FILTER};
  uint8_t channel_{0};
};

}  // namespace dsp408
}  // namespace esphome
