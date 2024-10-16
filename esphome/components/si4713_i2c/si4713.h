#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "si4713defs.h"

namespace esphome {
namespace si4713 {

#ifndef SUB_TEXT
#define SUB_TEXT(name) \
 protected: \
  text::Text *name##_text_{nullptr}; \
\
 public: \
  void set_##name##_text(text::Text *text) { this->name##_text_ = text; }
#endif

class Si4713Component : public PollingComponent, public i2c::I2CDevice {
  std::string chip_id_;
  InternalGPIOPin *reset_pin_;
  bool reset_;

  // config

  OpMode op_mode_;
  uint16_t frequency_;
  uint8_t power_;
  uint8_t antcap_;
  PropTxComponentEnable tx_component_enable_;
  PropTxAudioDeviation tx_audio_deviation_;
  PropTxPreEmphasis tx_pre_emphasis_;
  PropTxPilotFrequency tx_pilot_frequency_;
  PropTxPilotDeviation tx_pilot_deviation_;
  PropTxLineInputLevel tx_line_input_level_;
  PropTxLineInputMute tx_line_input_mute_;
  PropDigitalInputFormat digital_input_format_;
  PropDigitalInputSampleRate digital_input_sample_rate_;
  PropRefClkFreq refclk_freq_;
  PropRefClkPreScale refclk_prescale_;
  PropTxAcompEnable tx_acomp_enable_;
  PropTxAcompThreshold tx_acomp_threshold_;
  PropTxAcompAttackTime tx_acomp_attack_time_;
  PropTxAcompReleaseTime tx_acomp_release_time_;
  AcompPreset tx_acomp_preset_;
  PropTxAcompGain tx_acomp_gain_;
  PropTxLimiterReleaseTime tx_limiter_releasee_time_;
  PropTxAsqInterruptSource tx_asq_interrupt_source_;
  PropTxAsqLevelLow tx_asq_level_low_;
  PropTxAsqDurationLow tx_asq_duration_low_;
  PropTxAsqLevelHigh tx_asq_level_high_;
  PropTxAsqDurationHigh tx_asq_duration_high_;
  PropTxRdsDeviation tx_rds_deviation_;
  uint8_t gpio_[3];

  // TODO

  /*
    // general
    op_mode: Analog / Analog, Digital
    mute: False
    mono: False
    pre_emphasis: 75us / 75us, 50us, Disabled, Reserved
    // tune
    frequency: 87.50 (MHz) / 0.05
    audio_deviation: 68.25 (kHz) 0 to 90 / .01
    power: 115 (dBuV) 88 to 115 / 1
    antcap: 0 (auto) 0 to 191 / 1
    // line input
    line_level: 636 (mVPK) / ? to ?
    line_attenuation: 60 (kOhm) / 190, 301, 416, 636 mV or 396, 100, 74, 60 kOhm
    // digital input
    digital_sample_rate: 44100 (Hz) / 32000 to 48000
    digital_sample_precision: 16 (bits) / 16, 20, 24, 8
    digital_channels: 0 / 0 stereo 1 mono
    digital_mode: Default / Default, I2S, Left-justified, MSB at 1st DCLK rising edge after DFS Pulse, MSB ... 2nd ...
    // stereo
    pilot_enable: True
    pilot_frequency: 19000 (Hz) 0 to 19000 / 1
    pilot_deviation: 6750 (Hz) 0 to 90000 / 1
    // refclk
    refclkf: 32768 (Hz) 31130 to 34406 / 1
    rclksel: RCLK / RCLK, DCLK
    rclkp: 1
    // compressor
    acomp_enable: False
    acomp_threshold: -40 (dBFS) -40 to 0 / 1
    acomp_attack: 0.5 (ms) 0.5 to 5 / .5
    acomp_release: 1000 (ms) / 100, 200, 350, 525, 1000
    acomp_gain: 15 (dB) / 0 to 20
    acomp_preset:
      Disabled => acomp_enable: False
      Minimal => acomp_enable: True, acomp_threshold: -40, acomp_attack: 5, acomp_release: 100, acomp_gain: 15
      Aggressive => acomp_enable: True, acomp_threshold: -15, acomp_attack: 0.5, acomp_release: 1000, acomp_gain: 5
    // limiter
    limiter_enable: True
    limiter_release_time: 5.01 (ms) / 5 = 102.39 ms .. 2000 = 0.25 ms
    // rds
    rds_enable: False
    rds_deviation: 2 (kHz) 0 to 7.5 / .01
    rds_station:
    rds_text:
  */
  
  bool send_cmd(const void *cmd, size_t cmd_size, void *res, size_t res_size);

  template<typename CMD> bool send_cmd(const CMD &cmd) {
    return this->send_cmd((const void *) &cmd, sizeof(cmd), nullptr, 0);
  }

  template<typename CMD, typename RES> bool send_cmd(CMD cmd, RES &res) {
    return this->send_cmd((const void *) &cmd, sizeof(cmd), (void *) &res, sizeof(res));
  }

  template<typename P> bool set_prop(P p) {
    CmdSetProperty cmd = CmdSetProperty(p.PROP, p.PROPD);
    return this->send_cmd(cmd);
  }

  template<typename P> bool get_prop(P &p) {
    ResGetProperty res;
    if (this->send_cmd(CmdGetProperty(p.PROP), res)) {
      p.PROPD = ((uint16_t) res.PROPDH << 8) | res.PROPDL;
      return true;
    }

    return false;
  }

  std::string rds_station_;
  std::string rds_text_;

  void rds_update_();

  bool reset();
  bool power_up();
  bool power_down();
  bool detect_chip_id();
  bool tune_freq(uint16_t freq);
  bool tune_power(uint8_t power = 0, uint8_t antcap = 0);
  bool tune_wait();

  // TODO: sensors TX_TUNE_STATUS / FREQ, RFuV, ANTCAP, NL
  // TODO: sensors TX_ASQ_STATUS / OVERMOD, IALH, IALL, INLEVEL
  
  // general config
  SUB_SWITCH(mute)
  SUB_SWITCH(mono)
  SUB_SELECT(pre_emphasis)
  // tuner
  SUB_NUMBER(frequency)
  SUB_NUMBER(audio_deviation)
  SUB_NUMBER(power)
  SUB_NUMBER(antcap)
  // analog
  SUB_NUMBER(analog_level)
  SUB_SELECT(analog_attenuation)
  // digital
  SUB_NUMBER(digital_sample_rate)
  SUB_SELECT(digital_sample_bits)
  SUB_SELECT(digital_channels)
  SUB_SELECT(digital_mode)
  SUB_SELECT(digital_clock_edge)
  // pilot
  SUB_SWITCH(pilot_enable)
  SUB_NUMBER(pilot_frequency)
  SUB_NUMBER(pilot_deviation)
  // refclk
  SUB_NUMBER(refclk_frequency)
  SUB_SELECT(refclk_source)
  SUB_NUMBER(refclk_prescaler)
  // compressor
  SUB_SWITCH(acomp_enable)
  SUB_NUMBER(acomp_threshold)
  SUB_SELECT(acomp_attack)
  SUB_SELECT(acomp_release)
  SUB_NUMBER(acomp_gain)
  SUB_SELECT(acomp_preset)
  // limiter
  SUB_SWITCH(limiter_enable)
  SUB_NUMBER(limiter_release_time)
  // asq
  SUB_SWITCH(asq_overmod_enable)
  SUB_SWITCH(asq_iall_enable)
  SUB_SWITCH(asq_ialh_enable)
  SUB_NUMBER(asq_level_low)
  SUB_NUMBER(asq_duration_low)
  SUB_NUMBER(asq_level_high)
  SUB_NUMBER(asq_duration_high)
  // rds
  SUB_SWITCH(rds_enable)
  SUB_NUMBER(rds_deviation)
  SUB_TEXT(rds_station)
  SUB_TEXT(rds_text)
  // output
  SUB_SWITCH(gpio1)
  SUB_SWITCH(gpio2)
  SUB_SWITCH(gpio3)
  // sensors
  SUB_TEXT_SENSOR(chip_id)

  void publish_mute();
  void publish_mono();
  void publish_pre_emphasis();
  void publish_frequency();
  void publish_audio_deviation();
  void publish_power();
  void publish_antcap();
  void publish_analog_level();
  void publish_analog_attenuation();
  void publish_digital_sample_rate();
  void publish_digital_sample_bits();
  void publish_digital_channels();
  void publish_digital_mode();
  void publish_digital_clock_edge();
  void publish_pilot_enable();
  void publish_pilot_frequency();
  void publish_pilot_deviation();
  void publish_refclk_frequency();
  void publish_refclk_source();
  void publish_refclk_prescaler();
  void publish_acomp_enable();
  void publish_acomp_threshold();
  void publish_acomp_attack();
  void publish_acomp_release();
  void publish_acomp_gain();
  void publish_acomp_preset();
  void publish_limiter_enable();
  void publish_limiter_release_time();
  void publish_asq_overmod_enable();
  void publish_asq_iall_enable();
  void publish_asq_ialh_enable();
  void publish_asq_level_low();
  void publish_asq_duration_low();
  void publish_asq_level_high();
  void publish_asq_duration_high();
  void publish_rds_enable();
  void publish_rds_deviation();
  void publish_rds_station();
  void publish_rds_text();
  void publish_gpio(uint8_t pin);
  void publish_chip_id();
  void publish(sensor::Sensor *s, float state);
  void publish(text_sensor::TextSensor *s, const std::string &state);
  void publish(number::Number *n, float state);
  void publish(switch_::Switch *s, bool state);
  void publish(select::Select *s, size_t index);
  void publish(text::Text *t, const std::string &state);

 public:
  Si4713Component();

  // float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  // non-mutable (opmode might be)
  void set_reset_pin(InternalGPIOPin *pin);
  void set_op_mode(OpMode value);

  void set_mute(bool value);
  bool get_mute();
  void set_mono(bool value);
  bool get_mono();
  void set_pre_emphasis(PreEmphasis value);
  PreEmphasis get_pre_emphasis();
  void set_frequency(float value);
  float get_frequency();
  void set_audio_deviation(float value);
  float get_audio_deviation();
  void set_power(int value);
  int get_power();
  void set_antcap(int value);
  int get_antcap();
  void set_analog_level(int value);
  int get_analog_level();
  void set_analog_attenuation(LineAttenuation value);
  LineAttenuation get_analog_attenuation();
  void set_digital_sample_rate(int value);
  int get_digital_sample_rate();
  void set_digital_sample_bits(SampleBits value);
  SampleBits get_digital_sample_bits();
  void set_digital_channels(SampleChannels value);
  SampleChannels get_digital_channels();
  void set_digital_mode(DigitalMode value);
  DigitalMode get_digital_mode();
  void set_digital_clock_edge(DigitalClockEdge value);
  DigitalClockEdge get_digital_clock_edge();
  void set_pilot_enable(bool value);
  bool get_pilot_enable();
  void set_pilot_frequency(float value);
  float get_pilot_frequency();
  void set_pilot_deviation(float value);
  float get_pilot_deviation();
  void set_refclk_frequency(int value);
  int get_refclk_frequency();
  void set_refclk_source(RefClkSource value);
  RefClkSource get_refclk_source();
  void set_refclk_prescaler(int value);
  int get_refclk_prescaler();
  void set_acomp_enable(bool value);
  bool get_acomp_enable();
  void set_acomp_threshold(int value);
  int get_acomp_threshold();
  void set_acomp_attack(AcompAttack value);
  AcompAttack get_acomp_attack();
  void set_acomp_release(AcompRelease value);
  AcompRelease get_acomp_release();
  void set_acomp_gain(int value);
  int get_acomp_gain();
  void set_acomp_preset(AcompPreset value);
  AcompPreset get_acomp_preset();
  void set_limiter_enable(bool value);
  bool get_limiter_enable();
  void set_limiter_release_time(float value);
  float get_limiter_release_time();
  void set_asq_overmod_enable(bool value);
  bool get_asq_overmod_enable();
  void set_asq_iall_enable(bool value);
  bool get_asq_iall_enable();
  void set_asq_ialh_enable(bool value);
  bool get_asq_ialh_enable();
  void set_asq_level_low(int value);
  int get_asq_level_low();
  void set_asq_duration_low(int value);
  int get_asq_duration_low();
  void set_asq_level_high(int value);
  int get_asq_level_high();
  void set_asq_duration_high(int value);
  int get_asq_duration_high();
  void set_rds_enable(bool value);
  bool get_rds_enable();
  void set_rds_deviation(float value);
  float get_rds_deviation();
  void set_rds_station(const std::string &value);
  void set_rds_text(const std::string &value);
  void set_gpio(uint8_t pin, bool value);
  bool get_gpio(uint8_t pin);
};

}  // namespace si4713
}  // namespace esphome
