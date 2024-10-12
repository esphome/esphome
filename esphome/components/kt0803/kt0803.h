#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "kt0803defs.h"

namespace esphome {
namespace kt0803 {

#ifndef SUB_TEXT
#define SUB_TEXT(name) \
 protected: \
  text::Text *name##_text_{nullptr}; \
\
 public: \
  void set_##name##_text(text::Text *text) { this->name##_text_ = text; }
#endif

class KT0803Component : public PollingComponent, public i2c::I2CDevice {
  ChipId chip_id_;  // no way to detect it
  bool reset_;
  union {
    struct KT0803State state_;
    uint8_t regs_[sizeof(struct KT0803State)];
  };

  bool check_reg_(uint8_t addr);
  void write_reg_(uint8_t addr);
  bool read_reg_(uint8_t addr);

  SUB_BINARY_SENSOR(pw_ok)
  SUB_BINARY_SENSOR(slncid)
  SUB_NUMBER(frequency)
  SUB_NUMBER(pga)
  SUB_NUMBER(rfgain)
  SUB_SWITCH(mute)
  SUB_SWITCH(mono)
  SUB_SELECT(pre_emphasis)
  SUB_SELECT(pilot_tone_amplitude)
  SUB_SELECT(bass_boost_control)
  SUB_SWITCH(alc_enable)
  SUB_SWITCH(auto_pa_down)
  SUB_SWITCH(pa_down)
  SUB_SWITCH(standby_enable)
  SUB_SELECT(alc_attack_time)
  SUB_SELECT(alc_decay_time)
  SUB_SWITCH(pa_bias)
  SUB_SELECT(audio_limiter_level)
  SUB_SELECT(switch_mode)
  SUB_SELECT(silence_high)
  SUB_SELECT(silence_low)
  SUB_SWITCH(silence_detection)
  SUB_SELECT(silence_duration)
  SUB_SELECT(silence_high_counter)
  SUB_SELECT(silence_low_counter)
  SUB_NUMBER(alc_gain)
  SUB_SELECT(xtal_sel)
  SUB_SWITCH(au_enhance)
  SUB_SELECT(frequency_deviation)

  void publish_pw_ok();
  void publish_slncid();
  void publish_frequency();
  void publish_pga();
  void publish_rfgain();
  void publish_mute();
  void publish_mono();
  void publish_pre_emphasis();
  void publish_pilot_tone_amplitude();
  void publish_bass_boost_control();
  void publish_alc_enable();
  void publish_auto_pa_down();
  void publish_pa_down();
  void publish_standby_enable();
  void publish_alc_attack_time();
  void publish_alc_decay_time();
  void publish_pa_bias();
  void publish_audio_limiter_level();
  void publish_switch_mode();
  void publish_silence_high();
  void publish_silence_low();
  void publish_silence_detection();
  void publish_silence_duration();
  void publish_silence_high_counter();
  void publish_silence_low_counter();
  void publish_alc_gain();
  void publish_xtal_sel();
  void publish_au_enhance();
  void publish_frequency_deviation();

  void publish(sensor::Sensor *s, float state);
  void publish(binary_sensor::BinarySensor *s, bool state);
  void publish(text_sensor::TextSensor *s, const std::string &state);
  void publish(number::Number *n, float state);
  void publish(switch_::Switch *s, bool state);
  void publish(select::Select *s, size_t index);
  void publish(text::Text *t, const std::string &state);

 public:
  KT0803Component();

  // float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_chip_id(ChipId value);
  ChipId get_chip_id();
  std::string get_chip_string() const;

  void set_frequency(float value);  // MHz
  float get_frequency();
  void set_pga(float value);
  float get_pga();
  void set_rfgain(float value);
  float get_rfgain();
  void set_mute(bool value);
  bool get_mute();
  void set_mono(bool value);
  bool get_mono();
  void set_pre_emphasis(PreEmphasis value);
  PreEmphasis get_pre_emphasis();
  void set_pilot_tone_amplitude(PilotToneAmplitude value);
  PilotToneAmplitude get_pilot_tone_amplitude();
  void set_bass_boost_control(BassBoostControl value);
  BassBoostControl get_bass_boost_control();
  void set_alc_enable(bool value);
  bool get_alc_enable();
  void set_auto_pa_down(bool value);
  bool get_auto_pa_down();
  void set_pa_down(bool value);
  bool get_pa_down();
  void set_standby_enable(bool value);
  bool get_standby_enable();
  void set_alc_attack_time(AlcTime value);
  AlcTime get_alc_attack_time();
  void set_alc_decay_time(AlcTime value);
  AlcTime get_alc_decay_time();
  void set_pa_bias(bool value);
  bool get_pa_bias();
  void set_audio_limiter_level(AudioLimiterLevel value);
  AudioLimiterLevel get_audio_limiter_level();
  void set_switch_mode(SwitchMode value);
  SwitchMode get_switch_mode();
  void set_silence_high(SilenceHigh value);
  SilenceHigh get_silence_high();
  void set_silence_low(SilenceLow value);
  SilenceLow get_silence_low();
  void set_silence_detection(bool value);
  bool get_silence_detection();
  void set_silence_duration(SilenceLowAndHighLevelDurationTime value);
  SilenceLowAndHighLevelDurationTime get_silence_duration();
  void set_silence_high_counter(SilenceHighLevelCounter value);
  SilenceHighLevelCounter get_silence_high_counter();
  void set_silence_low_counter(SilenceLowLevelCounter value);
  SilenceLowLevelCounter get_silence_low_counter();
  void set_alc_gain(float value);
  float get_alc_gain();
  void set_xtal_sel(XtalSel value);
  XtalSel get_xtal_sel();
  void set_au_enhance(bool value);
  bool get_au_enhance();
  void set_frequency_deviation(FrequencyDeviation value);
  FrequencyDeviation get_frequency_deviation();
};

}  // namespace kt0803
}  // namespace esphome
