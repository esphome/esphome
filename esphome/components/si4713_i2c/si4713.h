#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
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

#define SUB_NUMBER_EX(name) \
  SUB_NUMBER(name) \
  void publish_##name() { this->publish(this->name##_number_, (float) this->get_##name()); }

#define SUB_SWITCH_EX(name) \
  SUB_SWITCH(name) \
  void publish_##name() { this->publish_switch(this->name##_switch_, this->get_##name()); }

#define SUB_SELECT_EX(name) \
  SUB_SELECT(name) \
  void publish_##name() { this->publish_select(this->name##_select_, (size_t) this->get_##name()); }

#define SUB_TEXT_EX(name) \
  SUB_TEXT(name) \
  void publish_##name() { this->publish(this->name##_text_, this->get_##name()); }

#define SUB_SENSOR_EX(name) \
  SUB_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_sensor_, (float) this->get_##name()); }

#define SUB_BINARY_SENSOR_EX(name) \
  SUB_BINARY_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_binary_sensor_, this->get_##name()); }

#define SUB_TEXT_SENSOR_EX(name) \
  SUB_TEXT_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_text_sensor_, this->get_##name()); }

class Si4713Component : public PollingComponent, public i2c::I2CDevice {
  std::string chip_id_;
  InternalGPIOPin *reset_pin_;
  bool reset_;
  ResTxTuneStatus tune_status_;
  ResTxAsqStatus asq_status_;

  // config state

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
  std::string rds_station_;
  std::string rds_text_;
  uint8_t gpio_[3];

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

  void rds_update_();  // TODO

  bool reset();
  bool power_up();
  bool power_down();
  bool detect_chip_id();
  bool tune_freq(uint16_t freq);
  bool tune_power(uint8_t power, uint8_t antcap = 0);
  bool stc_wait();

  template<class S, class T> void publish(S *s, T state);
  // template specialization here is not supported by the compiler yet
  void publish_switch(switch_::Switch *s, bool state);
  void publish_select(select::Select *s, size_t index);

  // general config
  SUB_SWITCH_EX(mute)
  SUB_SWITCH_EX(mono)
  SUB_SELECT_EX(pre_emphasis)
  // tuner
  SUB_NUMBER_EX(frequency)
  SUB_NUMBER_EX(audio_deviation)
  SUB_NUMBER_EX(power)
  SUB_NUMBER_EX(antcap)
  // analog
  SUB_NUMBER_EX(analog_level)
  SUB_SELECT_EX(analog_attenuation)
  // digital
  SUB_NUMBER_EX(digital_sample_rate)
  SUB_SELECT_EX(digital_sample_bits)
  SUB_SELECT_EX(digital_channels)
  SUB_SELECT_EX(digital_mode)
  SUB_SELECT_EX(digital_clock_edge)
  // pilot
  SUB_SWITCH_EX(pilot_enable)
  SUB_NUMBER_EX(pilot_frequency)
  SUB_NUMBER_EX(pilot_deviation)
  // refclk
  SUB_NUMBER_EX(refclk_frequency)
  SUB_SELECT_EX(refclk_source)
  SUB_NUMBER_EX(refclk_prescaler)
  // compressor
  SUB_SWITCH_EX(acomp_enable)
  SUB_NUMBER_EX(acomp_threshold)
  SUB_SELECT_EX(acomp_attack)
  SUB_SELECT_EX(acomp_release)
  SUB_NUMBER_EX(acomp_gain)
  SUB_SELECT_EX(acomp_preset)
  // limiter
  SUB_SWITCH_EX(limiter_enable)
  SUB_NUMBER_EX(limiter_release_time)
  // asq
  SUB_SWITCH_EX(asq_iall_enable)
  SUB_SWITCH_EX(asq_ialh_enable)
  SUB_SWITCH_EX(asq_overmod_enable)
  SUB_NUMBER_EX(asq_level_low)
  SUB_NUMBER_EX(asq_duration_low)
  SUB_NUMBER_EX(asq_level_high)
  SUB_NUMBER_EX(asq_duration_high)
  // rds
  SUB_SWITCH_EX(rds_enable)
  SUB_NUMBER_EX(rds_deviation)
  SUB_TEXT_EX(rds_station)
  SUB_TEXT_EX(rds_text)
  // output
  SUB_SWITCH_EX(gpio1)
  SUB_SWITCH_EX(gpio2)
  SUB_SWITCH_EX(gpio3)
  // sensors
  SUB_TEXT_SENSOR_EX(chip_id)
  SUB_SENSOR_EX(read_frequency)
  SUB_SENSOR_EX(read_power)
  SUB_SENSOR_EX(read_antcap)
  SUB_SENSOR_EX(read_noise_level)
  SUB_BINARY_SENSOR_EX(iall)
  SUB_BINARY_SENSOR_EX(ialh)
  SUB_BINARY_SENSOR_EX(overmod)
  SUB_SENSOR_EX(inlevel)

  void publish_gpio(uint8_t pin);  // helper

 public:
  Si4713Component();

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  // non-mutable config (opmode could be)
  void set_reset_pin(InternalGPIOPin *pin);
  void set_op_mode(OpMode value);

  // config
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
  void set_asq_iall_enable(bool value);
  bool get_asq_iall_enable();
  void set_asq_ialh_enable(bool value);
  bool get_asq_ialh_enable();
  void set_asq_overmod_enable(bool value);
  bool get_asq_overmod_enable();
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
  std::string get_rds_station();
  void set_rds_text(const std::string &value);
  std::string get_rds_text();
  void set_gpio(uint8_t pin, bool value);  // helper
  bool get_gpio(uint8_t pin);              // helper
  void set_gpio1(bool value);
  bool get_gpio1();
  void set_gpio2(bool value);
  bool get_gpio2();
  void set_gpio3(bool value);
  bool get_gpio3();

  // used by sensors
  std::string get_chip_id();
  float get_read_frequency();
  float get_read_power();
  float get_read_antcap();
  float get_read_noise_level();
  bool get_iall();
  bool get_ialh();
  bool get_overmod();
  float get_inlevel();

  // used by automation
  void measure_freq(float value);
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...>, public Parented<Si4713Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->set_frequency(this->frequency_.value(x...)); }
};

template<typename... Ts> class MeasureFrequencyAction : public Action<Ts...>, public Parented<Si4713Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->measure_freq(this->frequency_.value(x...)); }
};

}  // namespace si4713
}  // namespace esphome
