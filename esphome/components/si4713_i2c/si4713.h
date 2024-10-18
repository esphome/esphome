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

#define SUB_NUMBER_EX(name, type) \
  SUB_NUMBER(name) \
  void publish_##name() { this->publish(this->name##_number_, (float) this->get_##name()); } \
  void set_##name(type value); \
  type get_##name();

#define SUB_SWITCH_EX(name) \
  SUB_SWITCH(name) \
  void publish_##name() { this->publish_switch(this->name##_switch_, this->get_##name()); } \
  void set_##name(bool value); \
  bool get_##name();

#define SUB_SELECT_EX(name, type) \
  SUB_SELECT(name) \
  void publish_##name() { this->publish_select(this->name##_select_, (size_t) this->get_##name()); } \
  void set_##name(type value); \
  type get_##name();

#define SUB_TEXT_EX(name) \
  SUB_TEXT(name) \
  void publish_##name() { this->publish(this->name##_text_, this->get_##name()); } \
  void set_##name(const std::string &value); \
  std::string get_##name();

#define SUB_SENSOR_EX(name) \
  SUB_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_sensor_, (float) this->get_##name()); } \
  float get_##name();

#define SUB_BINARY_SENSOR_EX(name) \
  SUB_BINARY_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_binary_sensor_, this->get_##name()); } \
  bool get_##name();

#define SUB_TEXT_SENSOR_EX(name) \
  SUB_TEXT_SENSOR(name) \
  void publish_##name() { this->publish(this->name##_text_sensor_, this->get_##name()); } \
  std::string get_##name();

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

 public:
  Si4713Component();

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_reset_pin(InternalGPIOPin *pin);
  void set_op_mode(OpMode value);

  SUB_SWITCH_EX(mute)
  SUB_SWITCH_EX(mono)
  SUB_SELECT_EX(pre_emphasis, PreEmphasis)
  SUB_NUMBER_EX(frequency, float)
  SUB_NUMBER_EX(audio_deviation, float)
  SUB_NUMBER_EX(power, int)
  SUB_NUMBER_EX(antcap, int)
  SUB_NUMBER_EX(analog_level, int)
  SUB_SELECT_EX(analog_attenuation, LineAttenuation)
  SUB_NUMBER_EX(digital_sample_rate, int)
  SUB_SELECT_EX(digital_sample_bits, SampleBits)
  SUB_SELECT_EX(digital_channels, SampleChannels)
  SUB_SELECT_EX(digital_mode, DigitalMode)
  SUB_SELECT_EX(digital_clock_edge, DigitalClockEdge)
  SUB_SWITCH_EX(pilot_enable)
  SUB_NUMBER_EX(pilot_frequency, float)
  SUB_NUMBER_EX(pilot_deviation, float)
  SUB_NUMBER_EX(refclk_frequency, int)
  SUB_SELECT_EX(refclk_source, RefClkSource)
  SUB_NUMBER_EX(refclk_prescaler, int)
  SUB_SWITCH_EX(acomp_enable)
  SUB_NUMBER_EX(acomp_threshold, int)
  SUB_SELECT_EX(acomp_attack, AcompAttack)
  SUB_SELECT_EX(acomp_release, AcompRelease)
  SUB_NUMBER_EX(acomp_gain, int)
  SUB_SELECT_EX(acomp_preset, AcompPreset)
  SUB_SWITCH_EX(limiter_enable)
  SUB_NUMBER_EX(limiter_release_time, float)
  SUB_SWITCH_EX(asq_iall_enable)
  SUB_SWITCH_EX(asq_ialh_enable)
  SUB_SWITCH_EX(asq_overmod_enable)
  SUB_NUMBER_EX(asq_level_low, int)
  SUB_NUMBER_EX(asq_duration_low, int)
  SUB_NUMBER_EX(asq_level_high, int)
  SUB_NUMBER_EX(asq_duration_high, int)
  SUB_SWITCH_EX(rds_enable)
  SUB_NUMBER_EX(rds_deviation, float)
  SUB_TEXT_EX(rds_station)
  SUB_TEXT_EX(rds_text)
  SUB_SWITCH_EX(gpio1)
  SUB_SWITCH_EX(gpio2)
  SUB_SWITCH_EX(gpio3)
  SUB_TEXT_SENSOR_EX(chip_id)
  SUB_SENSOR_EX(read_frequency)
  SUB_SENSOR_EX(read_power)
  SUB_SENSOR_EX(read_antcap)
  SUB_SENSOR_EX(read_noise_level)
  SUB_BINARY_SENSOR_EX(iall)
  SUB_BINARY_SENSOR_EX(ialh)
  SUB_BINARY_SENSOR_EX(overmod)
  SUB_SENSOR_EX(inlevel)

  // helper
  void publish_gpio(uint8_t pin);
  void set_gpio(uint8_t pin, bool value);
  bool get_gpio(uint8_t pin);

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
