#pragma once

#include <driver/i2s.h>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace i2s_audio {

class I2SAudioComponent;

class I2SAudioIn : public Parented<I2SAudioComponent> {};

// class I2SAudioOut {
//  public:
//   virtual void start() = 0;
//   virtual void stop() = 0;
// #if SOC_I2S_SUPPORTS_DAC
//   void set_internal_dac_mode(i2s_dac_mode_t mode) { this->internal_dac_mode_ = mode; }
//   i2s_dac_mode_t get_internal_dac_mode() const { return this->internal_dac_mode_; }
// #endif
//   void set_external_dac_channels(uint8_t channels) { this->external_dac_channels_ = channels; }

//  protected:
// #if SOC_I2S_SUPPORTS_DAC
//   i2s_dac_mode_t internal_dac_mode_{I2S_DAC_CHANNEL_DISABLE};
// #endif
//   uint8_t external_dac_channels_;
// };

class I2SAudioComponent : public Component {
 public:
  void setup() override;

  void register_audio_in(I2SAudioIn *in) {
    this->audio_in_ = in;
    in->set_parent(this);
  }
  // void register_audio_out(I2SAudioOut *out) { this->audio_out_ = out; }

  i2s_pin_config_t get_pin_config() const {
    return {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = this->bclk_pin_,
        .ws_io_num = this->lrclk_pin_,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };
  }

  void set_bclk_pin(uint8_t pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(uint8_t pin) { this->lrclk_pin_ = pin; }

 protected:
  I2SAudioIn *audio_in_{nullptr};
  // I2SAudioOut *audio_out_{nullptr};

  uint8_t bclk_pin_;
  uint8_t lrclk_pin_;
};

}  // namespace i2s_audio
}  // namespace esphome
