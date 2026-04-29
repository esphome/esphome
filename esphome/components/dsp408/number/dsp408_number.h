#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "esphome/components/number/number.h"
#include "../dsp408.h"

namespace esphome {
namespace dsp408 {

enum class NumberKind : uint8_t {
  MASTER_VOLUME = 0,
  CHANNEL_VOLUME,
  CHANNEL_DELAY,
  CHANNEL_HPF_FREQ,
  CHANNEL_LPF_FREQ,
  CHANNEL_COMP_ATTACK,
  CHANNEL_COMP_RELEASE,
  CHANNEL_COMP_THRESHOLD,
};

class DSP408Number : public number::Number, public Component {
 public:
  void set_parent(DSP408 *parent) { this->parent_ = parent; }
  void set_master_volume() { this->kind_ = NumberKind::MASTER_VOLUME; }
  void set_channel_volume(uint8_t ch) {
    this->kind_ = NumberKind::CHANNEL_VOLUME;
    this->channel_ = ch;
  }
  void set_channel_delay(uint8_t ch) {
    this->kind_ = NumberKind::CHANNEL_DELAY;
    this->channel_ = ch;
  }
  void set_channel_hpf_freq(uint8_t ch) {
    this->kind_ = NumberKind::CHANNEL_HPF_FREQ;
    this->channel_ = ch;
  }
  void set_channel_lpf_freq(uint8_t ch) {
    this->kind_ = NumberKind::CHANNEL_LPF_FREQ;
    this->channel_ = ch;
  }
  void set_channel_comp_attack(uint8_t ch) {
    this->kind_ = NumberKind::CHANNEL_COMP_ATTACK;
    this->channel_ = ch;
  }
  void set_channel_comp_release(uint8_t ch) {
    this->kind_ = NumberKind::CHANNEL_COMP_RELEASE;
    this->channel_ = ch;
  }
  void set_channel_comp_threshold(uint8_t ch) {
    this->kind_ = NumberKind::CHANNEL_COMP_THRESHOLD;
    this->channel_ = ch;
  }

  void setup() override {}
  void dump_config() override;

 protected:
  void control(float value) override;

  DSP408 *parent_{nullptr};
  NumberKind kind_{NumberKind::MASTER_VOLUME};
  uint8_t channel_{0};
};

}  // namespace dsp408
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2/S3/P4
