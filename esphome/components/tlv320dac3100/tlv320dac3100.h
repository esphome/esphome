#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::tlv320dac3100 {

enum class TLV320DAC3100Output : uint8_t { SPEAKER, HEADPHONE, BOTH };

class TLV320DAC3100 final : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override { return this->is_muted_; }
  float volume() override { return this->volume_; }
  void on_audio_started() override;

  void set_clock(uint8_t mux, uint8_t pll_p, uint8_t pll_r, uint8_t pll_j, uint16_t pll_d, uint8_t ndac, uint8_t mdac,
                 uint16_t dosr, uint8_t processing_block) {
    this->clock_mux_ = mux;
    this->pll_p_ = pll_p;
    this->pll_r_ = pll_r;
    this->pll_j_ = pll_j;
    this->pll_d_ = pll_d;
    this->ndac_ = ndac;
    this->mdac_ = mdac;
    this->dosr_ = dosr;
    this->processing_block_ = processing_block;
  }
  void set_word_length(uint8_t word_length) { this->word_length_ = word_length; }
  void set_output(TLV320DAC3100Output output) { this->output_ = output; }
  void set_speaker_gain(uint8_t speaker_gain) { this->speaker_gain_ = speaker_gain; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }

 protected:
  bool write_page_(uint8_t page, uint8_t reg, uint8_t value);
  bool write_mute_();
  bool write_volume_();
  void power_up_dac_();
  bool clocked_from_bclk_() const;

  bool has_speaker_() const { return this->output_ != TLV320DAC3100Output::HEADPHONE; }
  bool has_headphone_() const { return this->output_ != TLV320DAC3100Output::SPEAKER; }

  GPIOPin *reset_pin_{nullptr};
  float volume_{1.0f};
  uint16_t pll_d_{0};
  uint16_t dosr_{128};
  uint8_t clock_mux_{0};
  uint8_t pll_p_{1};
  uint8_t pll_r_{1};
  uint8_t pll_j_{4};
  uint8_t ndac_{1};
  uint8_t mdac_{1};
  uint8_t processing_block_{1};
  uint8_t word_length_{0};
  uint8_t speaker_gain_{0};
  TLV320DAC3100Output output_{TLV320DAC3100Output::BOTH};
  bool outputs_ready_{false};
  bool clock_running_{false};
  bool dac_powered_{false};
};

}  // namespace esphome::tlv320dac3100
