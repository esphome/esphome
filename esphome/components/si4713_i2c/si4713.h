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
#include "si4713sub.h"

namespace esphome {
namespace si4713_i2c {

class Si4713Component : public PollingComponent, public i2c::I2CDevice {
  std::string chip_id_;
  InternalGPIOPin *reset_pin_;
  bool reset_;
  ResTxTuneStatus tune_status_;
  ResTxAsqStatus asq_status_;

  // config state

  OpMode op_mode_;
  bool power_enable_;
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

  bool send_cmd_(const CmdBase *cmd, size_t cmd_size, ResBase *res, size_t res_size);

  template<typename CMD> bool send_cmd_(const CMD &cmd) {
    return this->send_cmd_(&cmd, sizeof(cmd), nullptr, 0);
  }

  template<typename CMD, typename RES> bool send_cmd_(CMD cmd, RES &res) {
    return this->send_cmd_(&cmd, sizeof(cmd), &res, sizeof(res));
  }

  template<typename P> bool set_prop_(P p) {
    CmdSetProperty cmd = CmdSetProperty(p.PROP, p.PROPD);
    return this->send_cmd_(cmd);
  }

  template<typename P> bool get_prop_(P &p) {
    ResGetProperty res;
    if (this->send_cmd_(CmdGetProperty(p.PROP), res)) {
      p.PROPD = ((uint16_t) res.PROPDH << 8) | res.PROPDL;
      return true;
    }

    return false;
  }

  void rds_update_();  // TODO

  bool device_reset_();
  bool power_up_();
  bool power_down_();
  bool detect_chip_id_();
  bool tune_freq_(uint16_t freq);
  bool tune_power_(uint8_t power, uint8_t antcap = 0);
  bool stc_wait_();

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

  SI4713_SUB_SWITCH(mute)
  SI4713_SUB_SWITCH(mono)
  SI4713_SUB_SELECT(pre_emphasis, PreEmphasis)
  SI4713_SUB_SWITCH(tuner_enable)
  SI4713_SUB_NUMBER(tuner_frequency, float)
  SI4713_SUB_NUMBER(tuner_deviation, float)
  SI4713_SUB_NUMBER(tuner_power, int)
  SI4713_SUB_NUMBER(tuner_antcap, float)
  SI4713_SUB_NUMBER(analog_level, int)
  SI4713_SUB_SELECT(analog_attenuation, LineAttenuation)
  SI4713_SUB_NUMBER(digital_sample_rate, int)
  SI4713_SUB_SELECT(digital_sample_bits, SampleBits)
  SI4713_SUB_SELECT(digital_channels, SampleChannels)
  SI4713_SUB_SELECT(digital_mode, DigitalMode)
  SI4713_SUB_SELECT(digital_clock_edge, DigitalClockEdge)
  SI4713_SUB_SWITCH(pilot_enable)
  SI4713_SUB_NUMBER(pilot_frequency, float)
  SI4713_SUB_NUMBER(pilot_deviation, float)
  SI4713_SUB_NUMBER(refclk_frequency, int)
  SI4713_SUB_SELECT(refclk_source, RefClkSource)
  SI4713_SUB_NUMBER(refclk_prescaler, int)
  SI4713_SUB_SWITCH(acomp_enable)
  SI4713_SUB_NUMBER(acomp_threshold, int)
  SI4713_SUB_SELECT(acomp_attack, AcompAttack)
  SI4713_SUB_SELECT(acomp_release, AcompRelease)
  SI4713_SUB_NUMBER(acomp_gain, int)
  SI4713_SUB_SELECT(acomp_preset, AcompPreset)
  SI4713_SUB_SWITCH(limiter_enable)
  SI4713_SUB_NUMBER(limiter_release_time, float)
  SI4713_SUB_SWITCH(asq_iall)
  SI4713_SUB_SWITCH(asq_ialh)
  SI4713_SUB_SWITCH(asq_overmod)
  SI4713_SUB_NUMBER(asq_level_low, int)
  SI4713_SUB_NUMBER(asq_duration_low, int)
  SI4713_SUB_NUMBER(asq_level_high, int)
  SI4713_SUB_NUMBER(asq_duration_high, int)
  SI4713_SUB_SWITCH(rds_enable)
  SI4713_SUB_NUMBER(rds_deviation, float)
  SI4713_SUB_TEXT(rds_station)
  SI4713_SUB_TEXT(rds_text)
  SI4713_SUB_SWITCH(output_gpio1)
  SI4713_SUB_SWITCH(output_gpio2)
  SI4713_SUB_SWITCH(output_gpio3)
  SI4713_SUB_TEXT_SENSOR(chip_id)
  SI4713_SUB_SENSOR(frequency)
  SI4713_SUB_SENSOR(power)
  SI4713_SUB_SENSOR(antcap)
  SI4713_SUB_SENSOR(noise_level)
  SI4713_SUB_BINARY_SENSOR(iall)
  SI4713_SUB_BINARY_SENSOR(ialh)
  SI4713_SUB_BINARY_SENSOR(overmod)
  SI4713_SUB_SENSOR(inlevel)

  // helper
  void publish_output_gpio(uint8_t pin);
  void set_output_gpio(uint8_t pin, bool value);
  bool get_output_gpio(uint8_t pin);

  // used by automation
  void measure_freq(float value);
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...>, public Parented<Si4713Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->set_tuner_frequency(this->frequency_.value(x...)); }
};

template<typename... Ts> class MeasureFrequencyAction : public Action<Ts...>, public Parented<Si4713Component> {
  TEMPLATABLE_VALUE(float, frequency)
  void play(Ts... x) override { this->parent_->measure_freq(this->frequency_.value(x...)); }
};

}  // namespace si4713_i2c
}  // namespace esphome
